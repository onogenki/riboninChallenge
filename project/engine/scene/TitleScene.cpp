#include "TitleScene.h"
#include "Camera.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "Object3dCommon.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "SceneRenderPipeline.h"
#include "SkyBox.h"
#include "TextureManager.h"
#include "Object3dRenderContext.h"
#include "PostEffect.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "SceneManager.h"
#include "Logger.h"
#include <dinput.h>
#include <algorithm>
#include <cmath>
using namespace MyMath;

namespace {
// 演出の秒数と終点はここだけを調整すれば変更できます。
constexpr float kFrameSeconds = 0.1f;
constexpr float kNoiseCoverSeconds = 0.1f;
constexpr float kPullBackSeconds = 5.1f;
constexpr float kNoiseFadeSeconds = 0.4f;
constexpr float kTiltDownSeconds = 1.25f;
const Vector3 kCameraStart{ 0.0f, 0.0f, -10.0f };
const Vector3 kCameraPulledBack{ 0.0f, 0.0f, -52.0f };
const Vector3 kCameraLookDown{ 0.68f, 0.35f, 0.0f };
}

// TitleSceneが所有する前方宣言型を、完全な型を読み込んだ場所で生成します。
TitleScene::TitleScene() = default;

// TitleSceneが所有する前方宣言型を、完全な型を読み込んだ場所で解放します。
TitleScene::~TitleScene() = default;

// TitleEditorへManager所有の一覧を貸し、生成・削除操作も同じManagerへ渡します。
TitleEditor::Context TitleScene::MakeTitleEditorContext()
{
	TitleEditor::Context context{};
	context.camera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	context.normalObjects = &titleObjects_.GetNormalObjects();
	context.animationObjects = &titleObjects_.GetAnimationObjects();
	context.sprites = &titleObjects_.GetSprites();
	context.directionalLight = &directionalLight_;
	context.pointLight = &pointLight_;
	context.spotLight = &spotLight_;
	context.baseNormalObjectCount = titleObjects_.GetBaseNormalObjectCount();
	context.baseAnimationObjectCount = titleObjects_.GetBaseAnimationObjectCount();
	context.baseSpriteCount = titleObjects_.GetBaseSpriteCount();
	context.addModel = [this](const std::string& fileName)
	{
		return titleObjects_.AddModel(object3dCommon, fileName);
	};
	context.addTexture = [this](const std::string& textureFilePath)
	{
		return titleObjects_.AddTexture(spriteCommon, textureFilePath);
	};
	context.clearAdded = [this]()
	{
		titleObjects_.ClearAdded();
	};
	return context;
}

// 最初のSpriteだけを作り、3D準備はそのSpriteを描いた後のUpdateへ回します。
void TitleScene::Initialize()
{
	isInitialized_ = false;
	isSpriteReady_ = false;
	hasDrawnTitleSprite_ = false;
	hasDrawnPreparedScene_ = false;
	presentationStep_ = PresentationStep::kFrames;
	preparationStep_ = PreparationStep::kRenderSystems;
	isFinished_ = false;
	titleFrameTimer_ = 0.0f;//最初のアニメーションの時間
	titleFrameIndex_ = 0;//アニメーションspriteの1枚目
	presentationTimer_ = 0.0f;
	PostEffect::GetInstance()->SetRandomNoise(false);
	if (!InitializeTitleSprites()) {
		Logger::Log("Title initialization failed: a required sprite texture could not be loaded.");
		preparationStep_ = PreparationStep::kFailed;
		return;
	}
	isSpriteReady_ = true;
}

// DirectX・Camera・Object3d共通設定を、TitleScene用に初期化します。
void TitleScene::InitializeRenderSystems()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);

	// Title専用のCameraManagerを作り、ほかのSceneのCamera状態を持ち込まないようにします。
	InitializeMainCamera(kCameraStart);
	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());
}

// Title画面の背景モデルを照らす三種類のLightを、初期値へ設定します。
void TitleScene::InitializeDefaultLighting()
{
	directionalLight_.direction = { 1.0f, -1.0f, 1.0f };
	directionalLight_.intensity = 0.25f;
	directionalLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	// 監視画面の板と仮床を、光の向きで真っ黒にしないための環境光です。
	directionalLight_.ambientColor = { 1.0f, 1.0f, 1.0f };
	directionalLight_.ambientIntensity = 0.7f;

	pointLight_.position = { 0.0f, 2.0f, 0.0f };
	pointLight_.intensity = 1.0f;
	pointLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLight_.radius = 10.0f;
	pointLight_.decay = 1.0f;

	spotLight_.position = { 2.0f, 1.25f, 0.0f };
	spotLight_.intensity = 4.0f;
	spotLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	spotLight_.distance = 7.0f;
	// SpotLightの方向は長さが1である前提なので、必ず正規化します。
	spotLight_.direction = Normalize({ -1.0f, -1.0f, 0.0f });
	spotLight_.decay = 2.0f;
	spotLight_.cosAngle = std::cos(0.45f);
	spotLight_.cosFalloffStart = 1.0f;
}

// TitleのSprite生成はManagerへ任せ、編集対象には上側の仮画像を選びます。
bool TitleScene::InitializeTitleSprites()
{
	spriteCommon = SpriteCommon::GetInstance();
	if (!titleObjects_.InitializeSprites(spriteCommon, DirectXCommon::GetInstance())) {
		return false;
	}
	titleEditor_.SelectSprite(1);
	return true;
}

// 1フレームに一つの準備段階だけを進め、全部成功した時だけ3Dを有効にします。
void TitleScene::PrepareNextTitleStep()
{
	switch (preparationStep_) {
	case PreparationStep::kRenderSystems:
		InitializeRenderSystems();
		InitializeDefaultLighting();
		preparationStep_ = PreparationStep::kModel;
		break;
	case PreparationStep::kModel:
		if (!titleObjects_.InitializeModel(object3dCommon)) {
			Logger::Log("Title preparation failed: the monitor or floor plane could not be loaded.");
			preparationStep_ = PreparationStep::kFailed;
			presentationStep_ = PresentationStep::kFailed;
			break;
		}
		preparationStep_ = PreparationStep::kSkyBoxAndEditor;
		break;
	case PreparationStep::kSkyBoxAndEditor:
		if (!InitializeSkyBoxAndEditorResources()) {
			Logger::Log("Title preparation failed: the skybox texture could not be loaded.");
			preparationStep_ = PreparationStep::kFailed;
			presentationStep_ = PresentationStep::kFailed;
			break;
		}
		preparationStep_ = PreparationStep::kComplete;
		isInitialized_ = true;
		break;
	case PreparationStep::kComplete:
	case PreparationStep::kFailed:
		break;
	}
}

// 最後の画像を見せ終えた後、全画面ノイズで2Dから3Dへの切替を隠します。
void TitleScene::StartTitleNoise()
{
	Logger::Log("Title presentation: 12 frames and 3D preparation complete; noise cover started.");
	presentationStep_ = PresentationStep::kNoiseCover;
	presentationTimer_ = 0.0f;
	mainCamera->SetTranslate(kCameraStart);
	mainCamera->SetRotate({ 0.0f, 0.0f, 0.0f });
	// 他の画面効果が残るとRandomNoiseが選ばれないため、切替中はノイズを単独で使います。
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->SetGrayscale(false);
	postEffect->SetSepia(false);
	postEffect->SetVignette(false);
	postEffect->SetSmoothing(false);
	postEffect->SetGaussianFilter(false);
	postEffect->SetRadialBlur(false);
	postEffect->SetDissolve(false);
	postEffect->SetLuminanceBasedOutline(false);
	postEffect->SetDepthBasedOutline(false);
	postEffect->SetRandomNoiseIntensity(1.0f);
	postEffect->SetRandomNoise(true);
}

// Titleを選んだ時だけ、SkyBox TextureとEditor用Resource一覧をまとめて準備します。
bool TitleScene::InitializeSkyBoxAndEditorResources()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	// DDSが無いままSkyBoxを描くと、未登録SRVを参照してしまいます。
	if (!TextureManager::GetInstance()->LoadTexture("Resources/debug/qwantani_moonrise_puresky_1k.dds")) {
		return false;
	}
	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(dxCommon, cameraManager->GetActiveCamera());
	skyBox_->SetTexture("Resources/debug/qwantani_moonrise_puresky_1k.dds");
	// Releaseには編集UIがないため、素材棚の走査・サムネイル読込を行いません。
#ifdef USE_IMGUI
	titleEditor_.ScanResourceShelf();
#endif
	return true;
}

// Spriteを先に更新し、初回描画後に3D準備を進めます。
void TitleScene::Update()
{
	// Game Viewが操作対象かどうかと入力機器は、このフレームで一度だけ取得します。
	const bool isGameViewActive = ImGuiManager::GetInstance()->IsGameViewActive();
	if (!isSpriteReady_) {
		// 素材欠損時は未生成のSpriteに触れず、Scene切替も許可しません。
		return;
	}
	if (hasDrawnTitleSprite_ && !isInitialized_ && preparationStep_ != PreparationStep::kFailed) {
		PrepareNextTitleStep();
	}
	UpdateSceneContent();
	if (isInitialized_) {
		UpdateEditorUi(isGameViewActive);
	}
	UpdateSceneTransition(isGameViewActive);
}

// 導入演出を進め、Camera更新後に描画対象を準備します。
void TitleScene::UpdateSceneContent()
{
	const float deltaTime = DirectXCommon::GetInstance()->GetDeltaTime();
	UpdateTitlePresentation(deltaTime);
	if (isInitialized_) {
		// Cameraを先に更新し、RenderContextが完成済みの3D対象だけを準備します。
		cameraManager->Update();
		Object3dRenderContext::NormalizeDirectionalLight(directionalLight_);
		Object3dRenderContext renderContext(
			cameraManager->GetActiveCamera(),
			directionalLight_,
			pointLight_,
			spotLight_);
		renderContext.UpdateObjects(titleObjects_.GetNormalObjects());
		renderContext.UpdateObjects(titleObjects_.GetAnimationObjects());
		skyBox_->Update();
	}

	titleObjects_.ResizeTitleSprites();
	for (auto& sprite : titleObjects_.GetSprites()) {
		sprite->Update();
	}
}

// 準備が終わるまで12枚を周回し、終わった周回の最終コマから監視画面の演出へ移ります。
void TitleScene::UpdateTitlePresentation(float deltaTime)
{
	if (presentationStep_ == PresentationStep::kFrames) {
		titleFrameTimer_ += deltaTime;
		if (titleFrameTimer_ < kFrameSeconds) {
			return;
		}
		titleFrameTimer_ -= kFrameSeconds;
		if (titleFrameIndex_ == TitleObjectManager::kTitleFrameCount - 1 &&
			isInitialized_ && hasDrawnPreparedScene_) {
			StartTitleNoise();
			return;
		}
		titleFrameIndex_ = (titleFrameIndex_ + 1) % TitleObjectManager::kTitleFrameCount;
		if (!titleObjects_.SetTitleFrame(titleFrameIndex_)) {
			Logger::Log("Title frame texture could not be loaded.");
			presentationStep_ = PresentationStep::kFailed;
		}
		return;
	}
	if (presentationStep_ == PresentationStep::kFailed) {
		return;
	}

	// 最初は映像だけを完全なノイズで覆い、2D Spriteから3Dの板への変更を隠します。
	if (presentationStep_ == PresentationStep::kNoiseCover) {
		presentationTimer_ += deltaTime;
		if (presentationTimer_ >= kNoiseCoverSeconds) {
			presentationStep_ = PresentationStep::kPullBack;
			presentationTimer_ = 0.0f;
			titleFrameTimer_ = 0.0f;
			titleFrameIndex_ = 0;
			if (!titleObjects_.SetTitleFrame(titleFrameIndex_)) {
				Logger::Log("Title presentation failed: the first monitor frame could not be loaded.");
				PostEffect::GetInstance()->SetRandomNoise(false);
				presentationStep_ = PresentationStep::kFailed;
				return;
			}
			Logger::Log("Title presentation: camera pull-back started.");
		}
		return;
	}

	// 3Dの監視画面も同じ12枚を繰り返し、映像が停止した板に見えないようにします。
	titleFrameTimer_ += deltaTime;
	if (titleFrameTimer_ >= kFrameSeconds) {
		titleFrameTimer_ -= kFrameSeconds;
		titleFrameIndex_ = (titleFrameIndex_ + 1) % TitleObjectManager::kTitleFrameCount;
		if (!titleObjects_.SetTitleFrame(titleFrameIndex_)) {
			Logger::Log("Title monitor frame texture could not be loaded.");
		}
	}
	if (presentationStep_ == PresentationStep::kPullBack) {
		// 途中で止まらず遠くの停止位置まで後退し、ノイズだけ先に消します。
		presentationTimer_ += deltaTime;
		const float t = std::clamp(presentationTimer_ / kPullBackSeconds, 0.0f, 1.0f);
		const float eased = 1.0f - (1.0f - t) * (1.0f - t);
		mainCamera->SetTranslate(Lerp(kCameraStart, kCameraPulledBack, eased));
		const float noiseFade = std::clamp(
			(kNoiseFadeSeconds - presentationTimer_) / kNoiseFadeSeconds, 0.0f, 1.0f);
		PostEffect::GetInstance()->SetRandomNoiseIntensity(noiseFade);
		if (noiseFade <= 0.0f) {
			PostEffect::GetInstance()->SetRandomNoise(false);
		}
		if (t >= 1.0f) {
			presentationStep_ = PresentationStep::kTiltDown;
			presentationTimer_ = 0.0f;
			Logger::Log("Title presentation: downward camera turn started.");
		}
	} else if (presentationStep_ == PresentationStep::kTiltDown) {
		// 停止位置を固定したまま斜め下へ回し、画面の板を視野から外します。
		presentationTimer_ += deltaTime;
		const float t = std::clamp(presentationTimer_ / kTiltDownSeconds, 0.0f, 1.0f);
		const float eased = t * t * (3.0f - 2.0f * t);
		mainCamera->SetTranslate(kCameraPulledBack);
		mainCamera->SetRotate(Lerp({ 0.0f, 0.0f, 0.0f }, kCameraLookDown, eased));
		if (t >= 1.0f) {
			presentationStep_ = PresentationStep::kReady;
			Logger::Log("Title presentation: camera stopped; Enter is enabled.");
		}
	}
}


// 更新済みの描画対象をEditorへ貸し、最後にViewport操作を受け付けます。
void TitleScene::UpdateEditorUi(bool isGameViewActive)
{
	// Titleの編集UIは、3DモデルとSpriteを更新した後の状態を表示します。
	ImGuiManager::GetInstance()->Begin("Title");
	titleEditor_.Draw(MakeTitleEditorContext());
	if (isGameViewActive && presentationStep_ == PresentationStep::kReady) {
		SceneEditor::UpdateViewportCamera(cameraManager ? cameraManager->GetActiveCamera() : nullptr);
	}
	ImGuiManager::GetInstance()->End();
}


// 入力先がGame Viewの時だけ、従来のキーで次のSceneを予約します。
void TitleScene::UpdateSceneTransition(bool isGameViewActive)
{
	// Game Viewが操作対象でない時は、Editor操作をScene切替入力として扱いません。
	if (!isGameViewActive || presentationStep_ != PresentationStep::kReady) {
		return;
	}

	Input* input = Input::GetInstance();
	// Debug用UIがある構成だけ、SpaceまたはPadの1ボタンでDebug Sceneを開きます。
#ifdef USE_IMGUI
	if (input->TriggerKey(DIK_SPACE) || input->IsPadButtonPressed(0, 1))
	{
		SceneManager::GetInstance()->ChangeScene("DEBUG");
	}
#endif

	// EnterまたはPadの3ボタンで、本編のStage1を開きます。
	if (input->TriggerKey(DIK_RETURN) || input->IsPadButtonPressed(0, 3))
	{
		SceneManager::GetInstance()->ChangeSceneWithLoading("STAGE1");
	}
}

// 白で描画先を消し、3Dが未完成の間もSpriteを毎フレーム表示します。
void TitleScene::Draw()
{
	// 共通PipelineがRenderTextureへの描画開始を行い、Titleは描画対象だけを並べます。
	const Vector4 whiteClearColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	SceneRenderPipeline::Begin(DirectXCommon::GetInstance(), &whiteClearColor);
	if (!isSpriteReady_) {
		// 失敗はログへ記録済みです。未生成の描画物を使わず、このFrameを閉じます。
		SceneRenderPipeline::End(DirectXCommon::GetInstance(), nullptr, false);
		return;
	}

	if (isInitialized_) {
		// 仮画像の下で完成済みの3Dを一度描き、初回描画の準備も済ませます。
		SceneRenderPipeline::DrawObjects(object3dCommon, titleObjects_.GetNormalObjects());
		SceneRenderPipeline::DrawObjects(object3dCommon, titleObjects_.GetAnimationObjects());
		skyBox_->Draw();
	}

	// 切替後は全画面の導入Spriteだけを外し、Editorで追加したSpriteは残します。
	const bool showLoadingSprites =
		presentationStep_ == PresentationStep::kFrames || presentationStep_ == PresentationStep::kFailed;
	SceneRenderPipeline::DrawSprites(
		spriteCommon,
		titleObjects_.GetSprites(),
		showLoadingSprites ? 0 : titleObjects_.GetBaseSpriteCount());

	// ImGuiのBegin/Endが未実行の準備中はDrawも省き、未生成DrawDataを渡しません。
	SceneRenderPipeline::End(
		DirectXCommon::GetInstance(),
		cameraManager ? cameraManager->GetActiveCamera() : nullptr,
		isInitialized_);
	hasDrawnTitleSprite_ = true;
	if (isInitialized_) {
		hasDrawnPreparedScene_ = true;
	}
}

// 成功・途中失敗のどちらでも、Titleが所有した資源を同じ順番で解放します。
void TitleScene::Finalize()
{
	// Titleを途中で離れた場合も、次のSceneへノイズを持ち越しません。
	PostEffect::GetInstance()->SetRandomNoise(false);
	//GPUの完了待ち
	DirectXCommon::GetInstance()->WaitForGPU();
	titleObjects_.Finalize();
	skyBox_.reset();
	titleEditor_.Finalize();
	mainCamera.reset();
	cameraManager.reset();
	isFinished_ = false;
	isInitialized_ = false;
	isSpriteReady_ = false;
	hasDrawnTitleSprite_ = false;
	hasDrawnPreparedScene_ = false;
	presentationStep_ = PresentationStep::kFrames;
	presentationTimer_ = 0.0f;
	titleFrameTimer_ = 0.0f;
	titleFrameIndex_ = 0;
	preparationStep_ = PreparationStep::kRenderSystems;

}
