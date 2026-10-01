#include "LoadingScene.h"

#include "DirectXCommon.h"
#include "PostEffect.h"
#include "SceneManager.h"
#include "SceneRenderPipeline.h"
#include "Sprite.h"
#include "SpriteCommon.h"

// LoadingSceneが前方宣言したSprite型を、この実装ファイルで生成・破棄します。
LoadingScene::LoadingScene() = default;
LoadingScene::~LoadingScene() = default;

// Loading中に前Sceneの色補正が残らないようにし、白い背景だけを準備します。
void LoadingScene::Initialize()
{
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);
	hasRequestedNextScene_ = false;
	nextSceneName_ = SceneManager::GetInstance()->TakeLoadingDestinationSceneName();
	if (nextSceneName_.empty()) {
		nextSceneName_ = "TITLE";
	}
	InitializeLoadingSprite();
}

// SpriteCommonは共通描画設定を所有し、LoadingSceneは自分の白い一枚だけを所有します。
void LoadingScene::InitializeLoadingSprite()
{
	spriteCommon = SpriteCommon::GetInstance();
	spriteCommon->Initialize(DirectXCommon::GetInstance());

	loadingSprite_ = std::make_unique<Sprite>();
	loadingSprite_->Initialize(spriteCommon, "resources/debug/white.png");
	loadingSprite_->SetPosition({ 0.0f, 0.0f });
	loadingSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	// 最初のDrawまでUpdateが呼ばれなくても、その時点の画面サイズで白く覆います。
	const DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	loadingSprite_->SetSize({
		static_cast<float>(dxCommon->GetClientWidth()),
		static_cast<float>(dxCommon->GetClientHeight()),
	});
	loadingSprite_->Update();
}

// 次Sceneの初期化中も、直前に描いた白画面がウィンドウへ残ります。
void LoadingScene::Update()
{
	if (!hasRequestedNextScene_) {
		// 通常起動はTitleへ、本編開始ではSceneManagerが指定したStage1などへ進みます。
		hasRequestedNextScene_ = SceneManager::GetInstance()->ChangeScene(
			nextSceneName_);
	}

	if (loadingSprite_) {
		// 全画面背景だけはSpriteの投影に使う実サイズへ合わせ、最大化時の赤い余白を防ぎます。
		const DirectXCommon* dxCommon = DirectXCommon::GetInstance();
		loadingSprite_->SetSize({
			static_cast<float>(dxCommon->GetClientWidth()),
			static_cast<float>(dxCommon->GetClientHeight()),
		});
		loadingSprite_->Update();
	}
}

// Loading中はDockやDebug UIを描かず、白い画面だけをSwapChainへ出します。
void LoadingScene::Draw()
{
	// Spriteが画面端へ届かない一瞬も赤を出さないよう、Loadingだけ描画先全体を白で消去します。
	const Vector4 whiteClearColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	SceneRenderPipeline::Begin(DirectXCommon::GetInstance(), &whiteClearColor);
	if (loadingSprite_) {
		spriteCommon->SetCommonDrawSetting();
		loadingSprite_->Draw();
	}
	SceneRenderPipeline::End(DirectXCommon::GetInstance(), nullptr, false);
}

// LoadingSceneは白背景だけを所有するため、それだけを解放します。
void LoadingScene::Finalize()
{
	loadingSprite_.reset();
}
