#include "SceneManager.h"
#include "CaptureManager.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include <utility>

// アプリ全体で共有する一つのSceneManagerを返します。
SceneManager* SceneManager::GetInstance()
{
	static SceneManager instance;
	return &instance;
}

void SceneManager::Update()
{
	// 予約中のSceneを先に確定し、その後に現在Sceneを一回だけ更新します。
	ApplyPendingSceneChange();

	if (scene_) {
		scene_->Update();
	}
	if (sceneChangeCooldownFrames_ > 0) {
		--sceneChangeCooldownFrames_;
	}
}

// 予約済みSceneを終了・初期化して、次フレームから通常更新できる状態へ切り替えます。
void SceneManager::ApplyPendingSceneChange()
{
	if (!nextScene_) {
		return;
	}

	isChangingScene_ = true;
	DirectXCommon::GetInstance()->WaitForGPU();
	// 旧SceneがGPU資源を使い終えた後に、Scene固有の所有物を解放します。
	if (scene_) {
		scene_->Finalize();
		scene_.reset();
	}
	DirectXCommon::GetInstance()->WaitForGPU();

	// Factoryが生成済みの次Sceneを現在Sceneへ移し、初期化を一度だけ行います。
	scene_ = std::move(nextScene_);
	currentSceneName_ = pendingSceneName_;
	pendingSceneName_.clear();
	scene_->Initialize();
	sceneChangeCooldownFrames_ = 6;
	isChangingScene_ = false;
}

void SceneManager::Draw()
{
	if (scene_)
	{
		scene_->Draw();
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

// Scene名ごとのLoading別名を作らず、共通LoadingSceneを経由してから目的のSceneへ移動します。
bool SceneManager::ChangeSceneWithLoading(const std::string& sceneName)
{
	if (sceneName.empty() || !ChangeScene("LOADING")) {
		return false;
	}

	loadingDestinationSceneName_ = sceneName;
	return true;
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
	DirectXCommon::GetInstance()->WaitForGPU();
	nextScene_.reset();
	pendingSceneName_.clear();
	loadingDestinationSceneName_.clear();
	if (scene_)
	{
		scene_->Finalize();
		scene_.reset();
	}
	DirectXCommon::GetInstance()->WaitForGPU();
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
