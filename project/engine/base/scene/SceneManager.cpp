#include "SceneManager.h"
#include "CaptureManager.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <algorithm>
#include <exception>
#include <utility>
#include <Windows.h>

namespace Engine
{

// アプリ全体で共有する一つのSceneManagerを返します。
SceneManager* SceneManager::GetInstance()
{
	static SceneManager instance;
	return &instance;
}

void SceneManager::Update()
{
	// 予約中のSceneを先に確定し、その後に現在Sceneを一回だけ更新します。
	if (!ApplyPendingSceneChange()) {
		return;
	}

	if (scene_) {
		scene_->Update();
	}
	UpdateFade();
	if (sceneChangeCooldownFrames_ > 0) {
		--sceneChangeCooldownFrames_;
	}
}

// 新しいSceneの初期化失敗時は、未完成のSceneをUpdate・Drawへ渡さず終了します。
bool SceneManager::ApplyPendingSceneChange()
{
	if (!nextScene_) {
		return true;
	}

	isChangingScene_ = true;
	Engine::DirectXCommon::GetInstance()->WaitForGPU();
	// 旧SceneがGPU資源を使い終えた後に、Scene固有の所有物を解放します。
	if (scene_) {
		scene_->Finalize();
		scene_.reset();
	}
	Engine::DirectXCommon::GetInstance()->WaitForGPU();

	// Factoryが生成済みの次Sceneを現在Sceneへ移し、初期化を一度だけ行います。
	scene_ = std::move(nextScene_);
	currentSceneName_ = pendingSceneName_;
	pendingSceneName_.clear();
	try {
		scene_->Initialize();
	} catch (const std::exception& error) {
		Logger::Log("Scene initialization failed: " + currentSceneName_ + ": " + error.what());
		scene_.reset();
		currentSceneName_.clear();
		isChangingScene_ = false;
		PostQuitMessage(1);
		return false;
	}
	destinationDrawn_ = false;
	sceneChangeCooldownFrames_ = kSceneChangeCooldownFrames;
	isChangingScene_ = false;
	return true;
}

// 現在Sceneを描いた後だけ撮影へ進め、描画前の状態を保存しません。
void SceneManager::Draw()
{
	if (scene_)
	{
		scene_->Draw();
		if (fadePhase_ == FadePhase::kWaitingForDestination &&
			currentSceneName_ == fadeDestination_ && scene_->IsReadyForTransition()) {
			destinationDrawn_ = true;
		}
		CaptureManager::GetInstance()->UpdateAfterDraw();
	}
}

// 指定名のSceneを次フレームに切り替える予約をし、失敗理由はログへ残します。
bool SceneManager::ChangeScene(const std::string& sceneName)
{
	if (sceneFactory_ == nullptr) {
		Logger::Log("Scene change failed: scene factory is not set.");
		return false;
	}
	if (nextScene_ != nullptr || isChangingScene_ || sceneChangeCooldownFrames_ > 0) {
		Logger::Log("Scene change failed: another scene transition is already pending.");
		return false;
	}

	std::unique_ptr<BaseScene> newScene = sceneFactory_->CreateScene(sceneName);
	if (!newScene) {
		Logger::Log("Scene change failed: could not create scene '" + sceneName + "'.");
		return false;
	}

	nextScene_ = std::move(newScene);
	pendingSceneName_ = sceneName;
	return true;
}

// 行き先が登録済みか先に確認し、無効な名前ではLoadingだけを表示し続けないようにします。
bool SceneManager::ChangeSceneWithLoading(const std::string& sceneName)
{
	const auto& sceneNames = GetAvailableSceneNames();
	if (sceneName.empty() || sceneName == "LOADING" ||
		std::find(sceneNames.begin(), sceneNames.end(), sceneName) == sceneNames.end()) {
		Logger::Log("Scene change with loading failed: invalid destination scene: '" + sceneName + "'.");
		return false;
	}
	if (!ChangeScene("LOADING")) {
		return false;
	}

	loadingDestinationSceneName_ = sceneName;
	return true;
}

// Sceneの通常描画を残したまま白く覆い、Loadingで準備完了を待つ遷移を開始します。
bool SceneManager::ChangeSceneWithFade(const std::string& sceneName)
{
	const auto& names = GetAvailableSceneNames();
	if (fadePhase_ != FadePhase::kNone || HasPendingScene() || sceneName == "LOADING" ||
		std::find(names.begin(), names.end(), sceneName) == names.end() || !PrepareFadeSprite()) {
		return false;
	}
	fadeDestination_ = sceneName;
	fadeAlpha_ = 0.0f;
	fadePhase_ = FadePhase::kFadeIn;
	return true;
}

// 白になる時間だけを固定し、読み込み中は時間ではなくSceneの準備完了を待ちます。
void SceneManager::UpdateFade()
{
	constexpr float kFadeSeconds = 0.35f;
	const float step = std::clamp(Engine::DirectXCommon::GetInstance()->GetDeltaTime() / kFadeSeconds, 0.0f, 1.0f);
	if (fadePhase_ == FadePhase::kFadeIn) {
		fadeAlpha_ = (std::min)(1.0f, fadeAlpha_ + step);
		if (fadeAlpha_ >= 1.0f && ChangeSceneWithLoading(fadeDestination_)) {
			fadePhase_ = FadePhase::kWaitingForDestination;
		}
	} else if (fadePhase_ == FadePhase::kWaitingForDestination) {
		if (destinationDrawn_ && currentSceneName_ == fadeDestination_) {
			fadePhase_ = FadePhase::kFadeOut;
		}
	} else if (fadePhase_ == FadePhase::kFadeOut) {
		fadeAlpha_ = (std::max)(0.0f, fadeAlpha_ - step);
		if (fadeAlpha_ <= 0.0f) {
			fadePhase_ = FadePhase::kNone;
			fadeDestination_.clear();
		}
	}
}

// Loadingと同じ白画像を使い、Sceneから独立したSpriteとして切替中も保持します。
bool SceneManager::PrepareFadeSprite()
{
	if (fadeSprite_) {
		return true;
	}
	constexpr const char* kWhiteTexture = "resources/debug/white.png";
	if (!Engine::TextureManager::GetInstance()->LoadTexture(kWhiteTexture)) {
		Logger::Log("Fade initialization failed: white texture could not be loaded.");
		return false;
	}
	Engine::SpriteCommon* common = Engine::SpriteCommon::GetInstance();
	if (!common->GetDxCommon() && !common->Initialize(Engine::DirectXCommon::GetInstance())) {
		return false;
	}
	auto sprite = std::make_unique<Engine::Sprite>();
	if (!sprite->Initialize(common, kWhiteTexture)) {
		return false;
	}
	sprite->SetPosition({ 0.0f, 0.0f });
	sprite->SetAnchorPoint({ 0.0f, 0.0f });
	fadeSprite_ = std::move(sprite);
	return true;
}

// Sceneの最後のSpriteより後へ置き、画面寸法と透明度を毎回更新します。
void SceneManager::DrawFadeOverlay()
{
	if (!fadeSprite_ || fadePhase_ == FadePhase::kNone || fadeAlpha_ <= 0.0f) {
		return;
	}
	Engine::DirectXCommon* dx = Engine::DirectXCommon::GetInstance();
	fadeSprite_->SetSize({ static_cast<float>(dx->GetClientWidth()), static_cast<float>(dx->GetClientHeight()) });
	fadeSprite_->SetColor({ 1.0f, 1.0f, 1.0f, fadeAlpha_ });
	fadeSprite_->Update();
	// 深度を消さない専用設定で重ね、深度ベースのPostEffectへ元の値を残します。
	Engine::SpriteCommon::GetInstance()->SetOverlayDrawSetting();
	fadeSprite_->Draw();
}

// LoadingSceneが切替先を受け取った時点で消し、次回のLoadingへ持ち越しません。
std::string SceneManager::TakeLoadingDestinationSceneName()
{
	std::string destinationSceneName = std::move(loadingDestinationSceneName_);
	loadingDestinationSceneName_.clear();
	return destinationSceneName;
}

// 現在のScene名を使い、通常の切替処理と同じ経路で最初から作り直します。
bool SceneManager::RestartCurrentScene()
{
	return !currentSceneName_.empty() && ChangeScene(currentSceneName_);
}

// アプリ終了時などに、現在と予約中のSceneを安全な順番で解放します。
void SceneManager::FinalizeCurrentScene()
{
	isChangingScene_ = true;
	Engine::DirectXCommon::GetInstance()->WaitForGPU();
	nextScene_.reset();
	pendingSceneName_.clear();
	loadingDestinationSceneName_.clear();
	fadeSprite_.reset();
	fadePhase_ = FadePhase::kNone;
	fadeDestination_.clear();
	fadeAlpha_ = 0.0f;
	if (scene_)
	{
		scene_->Finalize();
		scene_.reset();
	}
	Engine::DirectXCommon::GetInstance()->WaitForGPU();
	currentSceneName_.clear();
	sceneChangeCooldownFrames_ = 0;
	isChangingScene_ = false;
}

// SceneFactoryが登録したScene名一覧を返し、Factory未設定時は空一覧を返します。
const std::vector<std::string>& SceneManager::GetAvailableSceneNames() const
{
	static const std::vector<std::string> empty;
	return sceneFactory_ != nullptr ? sceneFactory_->GetSceneNames() : empty;
}

// SceneManagerの破棄時にも、残っているSceneをFinalizeしてから解放します。
SceneManager::~SceneManager()
{
	//最後のシーンの終了と開放
	if (scene_ || nextScene_)
	{
		FinalizeCurrentScene();
	}
}
}
