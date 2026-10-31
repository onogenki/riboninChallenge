#include "LoadingScene.h"

#include "DirectXCommon.h"
#include "Logger.h"
#include "PostEffect.h"
#include "SceneManager.h"
#include "SceneRenderPipeline.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <Windows.h>

// LoadingSceneが前方宣言したSprite型を、この実装ファイルで生成・破棄します。
LoadingScene::LoadingScene() = default;
LoadingScene::~LoadingScene() = default;

// 白背景の準備に失敗した時は、未登録画像のまま次Sceneへ進めません。
void LoadingScene::Initialize()
{
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);
	hasRequestedNextScene_ = false;
	nextSceneName_ = Engine::SceneManager::GetInstance()->TakeLoadingDestinationSceneName();
	if (nextSceneName_.empty()) {
		nextSceneName_ = "TITLE";
	}
	if (!InitializeLoadingSprite()) {
		PostQuitMessage(1);
	}
}

// SpriteCommonは共通描画設定を所有し、LoadingSceneは読込済みの白い一枚だけを所有します。
bool LoadingScene::InitializeLoadingSprite()
{
	const char* texturePath = "resources/debug/white.png";
	if (!Engine::TextureManager::GetInstance()->LoadTexture(texturePath)) {
		Logger::Log(std::string("LoadingScene initialization failed: white texture could not be loaded: ") + texturePath);
		return false;
	}
	spriteCommon_ = Engine::SpriteCommon::GetInstance();
	if (!spriteCommon_->Initialize(Engine::DirectXCommon::GetInstance())) {
		return false;
	}

	auto loadingSprite = std::make_unique<Engine::Sprite>();
	if (!loadingSprite->Initialize(spriteCommon_, texturePath)) {
		Logger::Log("LoadingScene initialization failed: white Sprite could not be prepared.");
		return false;
	}
	loadingSprite->SetPosition({ 0.0f, 0.0f });
	loadingSprite->SetAnchorPoint({ 0.0f, 0.0f });
	// 最初のDrawまでUpdateが呼ばれなくても、その時点の画面サイズで白く覆います。
	const Engine::DirectXCommon* dxCommon = Engine::DirectXCommon::GetInstance();
	loadingSprite->SetSize({
		static_cast<float>(dxCommon->GetClientWidth()),
		static_cast<float>(dxCommon->GetClientHeight()),
	});
	loadingSprite->Update();
	loadingSprite_ = std::move(loadingSprite);
	return true;
}

// 白Spriteの表示後に次Sceneを予約し、生成不能なら白画面で待ち続けず終了します。
void LoadingScene::Update()
{
	if (!loadingSprite_) {
		return;
	}
	Engine::SceneManager* sceneManager = Engine::SceneManager::GetInstance();
	// 切替待機中は予約できないため、Loadingを描画しながら次の受付フレームを待ちます。
	if (!hasRequestedNextScene_ && !sceneManager->HasPendingScene()) {
		// 通常起動はTitleへ、本編開始ではSceneManagerが指定したStage1などへ進みます。
		if (!sceneManager->ChangeScene(nextSceneName_)) {
			Logger::Log("LoadingScene transition failed: could not create destination scene '" + nextSceneName_ + "'.");
			PostQuitMessage(1);
			return;
		}
		hasRequestedNextScene_ = true;
	}

	// 全画面背景だけはSpriteの投影に使う実サイズへ合わせ、最大化時の赤い余白を防ぎます。
	const Engine::DirectXCommon* dxCommon = Engine::DirectXCommon::GetInstance();
	loadingSprite_->SetSize({
		static_cast<float>(dxCommon->GetClientWidth()),
		static_cast<float>(dxCommon->GetClientHeight()),
	});
	loadingSprite_->Update();
}

// 白Spriteを準備できた時だけ、ImGuiなしの白い画面をSwapChainへ出します。
void LoadingScene::Draw()
{
	if (!loadingSprite_) {
		return;
	}
	// Spriteが画面端へ届かない一瞬も赤を出さないよう、Loadingだけ描画先全体を白で消去します。
	const Vector4 whiteClearColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	SceneRenderPipeline::Begin(Engine::DirectXCommon::GetInstance(), &whiteClearColor);
	spriteCommon_->SetCommonDrawSetting();
	loadingSprite_->Draw();
	SceneRenderPipeline::End(Engine::DirectXCommon::GetInstance(), nullptr, false);
}

// LoadingSceneは白背景だけを所有するため、それだけを解放します。
void LoadingScene::Finalize()
{
	loadingSprite_.reset();
}
