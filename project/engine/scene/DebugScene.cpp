#include "DebugScene.h"
#include "Audio.h"
#include "Camera.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "Object3dCommon.h"
#include "ParticleEmitter.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "SkyBox.h"
#include "TextureManager.h"
#include "Object3dFactory.h"
#include "Object3dRenderContext.h"
#include "ParticleManager.h"
#include "PostEffect.h"
#include "ImGuiManager.h"
#include "Input.h"
#include"SceneManager.h"
#include "../debug/DebugAnimationPreview.h"
#include "../debug/DebugCollisionOverlay.h"
#include "../debug/DebugSceneRenderer.h"
#include "../debug/DebugViewportPlacement.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <dinput.h>
#include <utility>
using namespace MyMath;

namespace {
constexpr const char* kDebugLevelFileName = "scene";
constexpr const char* kDebugLevelFilePath = "resources/levels/scene.json";
}

// DebugSceneが所有する前方宣言型を、完全な型を読み込んだ場所で生成します。
DebugScene::DebugScene() = default;

// DebugSceneが所有する前方宣言型を、完全な型を読み込んだ場所で解放します。
DebugScene::~DebugScene() = default;

void DebugScene::Initialize()
{
	// DebugSceneの開始順は、描画基盤→Camera→素材→初期配置→Light→Debug補助機能です。
	InitializeRenderSystems();
	InitializeCameras();
	InitializeSceneResources();
	if (!InitializeInitialContent()) {
		return;
	}
	InitializeLightsAndEmitters();
	InitializeDebugTools();
}

// PostEffect・Object3d・Sprite・Particleの共通状態を、DebugScene開始時の値へそろえます。
void DebugScene::InitializeRenderSystems()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);
	ParticleManager::GetInstance()->ClearAllGroups();

	// 3Dと2Dの共通描画部は、Sceneへ入る時に同じDirectX環境で初期化します。
	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);
	spriteCommon = SpriteCommon::GetInstance();
	spriteCommon->Initialize(dxCommon);
}

// DebugScene専用のCameraを作り、ほかのSceneで使ったCamera状態を持ち込みません。
void DebugScene::InitializeCameras()
{
	// MainCameraは通常のGame Viewへ使います。
	InitializeMainCamera({ 0.0f, 0.0f, -10.0f });
	ParticleManager::GetInstance()->SetCameraManager(cameraManager.get());

	// 上から確認する補助Cameraも同じManagerへ登録します。
	upCamera_ = std::make_unique<Camera>();
	upCamera_->SetRotate({ 0.785f,0.0f,0.0f });
	upCamera_->SetTranslate({ 0.0f,5.0f,-5.0f });
	cameraManager->AddCamera("UpCamera", upCamera_.get());

	// Object3dの初期Cameraには、現在選んだMainCameraを渡します。
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());
}


// 初期Objectより先に、Texture・SkyBox・Audio・Particle素材を読み込みます。
void DebugScene::InitializeSceneResources()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	TextureManager::GetInstance()->LoadTexture("Resources/debug/monsterBall.png");//2枚目
	TextureManager::GetInstance()->LoadTexture("Resources/debug/grass.png");//terrainのpng

	// Debug用Particleの素材と発生Groupは、種類ごとの設定を持つ部品へ準備を任せます。
	debugParticleEffects_.InitializeResources();

	// skyBoxの背景
	TextureManager::GetInstance()->LoadTexture("Resources/debug/qwantani_moon_noon_puresky_1k.dds");

	// SkyBoxによるモデルへの環境反射
	object3dCommon->SetEnvironmentTexturePath("Resources/debug/qwantani_moon_noon_puresky_1k.dds");

	//Skybox
	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize(dxCommon, cameraManager->GetActiveCamera());
	// 添付されているDDSテクスチャのパスを指定する
	skyBox_->SetTexture("Resources/debug/qwantani_moon_noon_puresky_1k.dds");

	// Debug Sceneは音を即時再生せず、Game View表示後にStage1がBGMを開始します。
}


// Terrain・Animation確認モデル・scene.json・初期Spriteを、DebugScene所有の一覧へ追加します。
bool DebugScene::InitializeInitialContent()
{
	// Factoryでモデル読込とObject3d初期化を済ませ、Debug固有の配置だけをここで決めます。
	auto objPlane = Object3dFactory::Create(object3dCommon, "debug/terrain.obj");
	if (!objPlane) {
		return false;
	}
	objPlane->GetTransform().translate = { 1.0f, -2.0f, 10.0f };
	objectPlane_ = objPlane.get();
	normalObjects_.push_back(std::move(objPlane));

	// 人型・歩行Animation・手持ちWeaponは、Animation確認専用部品がまとめて準備します。
	if (!debugAnimationPreview_.Initialize(object3dCommon, animationObjects_)) {
		return false;
	}

	// Debugが最初から作成するモデルの直後へ、scene.jsonのモデルを追加します。
	levelRuntime_.Initialize(
		kDebugLevelFileName,
		kDebugLevelFilePath,
		normalObjects_.size());
	levelRuntime_.Reload(MakeLevelRuntimeContext());
	levelRuntime_.SynchronizeWatch();
	
	for (uint32_t i = 0; i < 1; ++i)
	{
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon, "Resources/debug/monsterBall.png");

		if (i % 2 == 0) {
			// 偶数番目にはUVチェッカーのPNGを設定
			sprite->SetTexture("Resources/debug/uvChecker.png");
		}
		Vector2 pos = { 0.0f + i * 0.0f, 0.0f + i * 50.0f };
		sprite->SetPosition(pos);

		sprites_.push_back(std::move(sprite));
	}
	return true;
}

// Debug画面全体のLightとParticle Emitterを、確認しやすい初期値で作成します。
void DebugScene::InitializeLightsAndEmitters()
{
	// 平行光源はOFF（Intensity = 0.0f）
	directionalLight_.direction = { 1.0f, -1.0f, 1.0f };
	directionalLight_.intensity = 0.0f;
	directionalLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 点光源はON（初期位置 0, 2, 0）
	pointLight_.position = { 0.0f, 2.0f, 0.0f };
	pointLight_.intensity = 1.0f;
	pointLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLight_.radius = 10.0f;
	pointLight_.decay = 1.0f;

	//スポットライト
	spotLight_.position = { 2.0f,1.25f,0.0f };
	spotLight_.intensity = 4.0f;
	spotLight_.color = { 1.0f,1.0f,1.0f,1.0f };
	spotLight_.distance = 7.0f;
	spotLight_.direction =
		Normalize({ -1.0f,-1.0f,0.0f });
	spotLight_.decay = 2.0f;
	spotLight_.cosAngle =
		std::cos(std::numbers::pi_v<float> / 3.0f);
	spotLight_.cosFalloffStart = 1.0f;

	//パーティクル
	//座標、回転、発生数、発生頻度[秒]
	emitterTransform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	//Circleパーティクル
	emitterCircle_ = std::make_unique<ParticleEmitter>("Circle", emitterTransform_, 1, 0.1f,false);
	//四角形のパーティクル（風に吹かれる方）
	emitterPlane_ = std::make_unique<ParticleEmitter>("Plane", emitterTransform_, 1, 0.3f,true);

	//最初はCircleにする
	activeEmitter_ = emitterCircle_.get();
}


// 初期ObjectをECSへ登録し、Shelf・Capture・自動確認を開始します。
void DebugScene::InitializeDebugTools()
{
	baseAnimationObjectCount_ = animationObjects_.size();
	baseSpriteCount_ = sprites_.size();
	for (const auto& object : normalObjects_) {
		if (object) {
			entityRegistry_.RegisterInitialModel(object.get(), object->GetModelName(), false);
		}
	}
	for (const auto& object : animationObjects_) {
		if (object) {
			entityRegistry_.RegisterInitialModel(object.get(), object->GetModelName(), true);
		}
	}
	for (const auto& sprite : sprites_) {
		if (sprite) {
			entityRegistry_.RegisterInitialSprite(sprite.get(), "Initial Sprite");
		}
	}
	levelRuntime_.SetEcsSyncReady(true);
	debugSceneEditor_.ScanResources();
	gameViewCapture_.Initialize();
	InitializeUiSmokeFromEnvironment();
	InitializeTimePlaybackSmokeFromEnvironment();
}

// 指定モデルを単独表示するPreviewへ切り替えます。
bool DebugScene::EnterModelPreview(const std::string& fileName)
{
	auto object = DebugSceneContent::CreateObject(MakeContentContext(), fileName, true);
	if (!object) {
		return false;
	}

	Camera* activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	// モデルごとの中心と半径はShelfのスキャン結果から受け取り、プレビュー部品へ渡します。
	Vector3 previewCenter{};
	float previewRadius = 1.0f;
	const std::vector<SceneEditor::ShelfEntry>& modelLibrary = debugSceneEditor_.GetEntries();
	auto modelIt = std::find_if(modelLibrary.begin(), modelLibrary.end(), [&](const SceneEditor::ShelfEntry& entry) {
		return entry.fileName == fileName;
	});
	if (modelIt != modelLibrary.end()) {
		previewCenter = modelIt->thumbnailCenter;
		previewRadius = (std::max)(modelIt->thumbnailRadius, 0.1f);
	}

	return assetPreview_.EnterModel(
		std::move(object),
		fileName,
		previewCenter,
		previewRadius,
		activeCamera);
}

// 指定Textureを単独表示するPreviewへ切り替えます。
bool DebugScene::EnterTexturePreview(const std::string& textureFilePath)
{
	return assetPreview_.EnterTexture(
		spriteCommon,
		textureFilePath,
		cameraManager ? cameraManager->GetActiveCamera() : nullptr);
}

// モデルまたはTextureのPreviewを終了し、通常のDebug Scene表示へ戻します。
void DebugScene::ExitModelPreview()
{
	assetPreview_.Exit(cameraManager ? cameraManager->GetActiveCamera() : nullptr);
}

// Debug用Particleを発生させる、人型Animationモデル付近の座標を返します。
Vector3 DebugScene::GetParticleEffectPosition() const
{
	return debugAnimationPreview_.GetParticleEffectPosition();
}

void DebugScene::Update()
{
	// DebugSceneの一フレームは、読込→Capture→内容→UI→自動確認の順です。
	// Blenderからscene.jsonが保存された時だけ、配置済みモデルを再読込します。
	levelRuntime_.UpdateHotReload(MakeLevelRuntimeContext());
	// UI自動確認中は、確認用の一枚保存だけを行います。
	// 通常のリプレイ蓄積を同時に走らせると、同じRenderTextureを二重に読み取ってしまいます。
	if (!DebugUiSmoke::IsEnabled(uiSmoke_)) {
		gameViewCapture_.Update();
	}
	UpdateSceneContent();
	UpdateEditorUi();
	UpdateAutomation();
	// 編集欄のBackSpaceと混同しないよう、Game View操作中だけTitleへ戻ります。
	if (ImGuiManager::GetInstance()->IsGameViewActive() &&
		Input::GetInstance()->TriggerKey(DIK_BACK)) {
		SceneManager::GetInstance()->ChangeSceneWithLoading("TITLE");
	}
}

// Camera・Particle・3D/2Dモデルを、現在のScene設定で更新します。
void DebugScene::UpdateSceneContent()
{
	gameViewCameraController_.Update(
		cameraManager ? cameraManager->GetActiveCamera() : nullptr,
		assetPreview_);

	// Cameraを先に更新してから、その行列を全モデルの描画準備へ使います。
	cameraManager->Update();

	// 選択中のEmitterだけを動かし、ParticleManagerは全Groupを更新します。
	if (activeEmitter_) {
		activeEmitter_->Update();
	}
	ParticleManager::GetInstance()->Update();

	// DirectionalLightは全Object3dで共有するため、一度だけ安全な単位方向へそろえます。
	Object3dRenderContext::NormalizeDirectionalLight(directionalLight_);
	Object3dRenderContext renderContext(
		cameraManager->GetActiveCamera(),
		directionalLight_,
		pointLight_,
		spotLight_);

	// 通常モデルへ、同じCamera・Light・行列更新をまとめて適用します。
	renderContext.UpdateObjects(normalObjects_);

	// 歩行確認・手持ちモデル・足跡ParticleはDebug専用部品へまとめて任せます。
	debugAnimationPreview_.Update({
		&animationObjects_,
		&renderContext,
		&directionalLight_,
		DirectXCommon::GetInstance()->GetDeltaTime(),
		ImGuiManager::GetInstance()->IsGameViewActive(),
	});

	if (Object3d* previewObject = assetPreview_.GetObject()) {
		renderContext.UpdateObject(*previewObject);
	}

	for (auto& sprite : sprites_)
	{
		sprite->Update();
	}
	entityRegistry_.Synchronize(normalObjects_, animationObjects_, sprites_);
	skyBox_->Update();
}

// Preview復帰、Editor、Skeleton表示を含むEdit Viewの操作を更新します。
void DebugScene::UpdateEditorUi()
{
	ImGuiManager::GetInstance()->Begin("Debug");
	// プレビューはEdit View専用とし、Game Viewへ戻った時は通常のシーン表示へ復帰する。
	if (ImGuiManager::GetInstance()->IsGameViewActive() && assetPreview_.IsActive()) {
		ExitModelPreview();
	}
	// Debug編集画面のUI配置とCollider表示は、専用部品へまとめて任せます。
	DebugSceneEditor::Context editorContext = MakeEditorContext();
	debugSceneEditor_.Draw(editorContext);
#ifdef USE_IMGUI
	if (assetPreview_.ShouldExitOnOutsideClick(
		ImGui::IsMouseDown(ImGuiMouseButton_Left),
		ImGui::IsMouseClicked(ImGuiMouseButton_Left),
		ImGuiManager::GetInstance()->IsMouseOverGameView(
			Input::GetInstance()->GetMouseScreen().x,
			Input::GetInstance()->GetMouseScreen().y))) {
		ExitModelPreview();
	}
#endif
	// Animation確認部品が所有する人型モデルのSkeletonを、必要な時だけ重ねて表示します。
	debugAnimationPreview_.DrawSkeletonDebug(
		cameraManager ? cameraManager->GetActiveCamera() : nullptr,
		ImGuiManager::GetInstance()->IsSkeletonDebugDrawEnabled());

	ImGuiManager::GetInstance()->End();
}

void DebugScene::Draw()
{
	// 通常SceneとPreviewの描画経路をDebug専用Rendererへ渡します。
	DebugSceneRenderer::Draw({
		DirectXCommon::GetInstance(),
		object3dCommon,
		spriteCommon,
		cameraManager ? cameraManager->GetActiveCamera() : nullptr,
		assetPreview_.IsActive() ? assetPreview_.GetObject() : nullptr,
		assetPreview_.IsActive() ? assetPreview_.GetSprite() : nullptr,
		&normalObjects_,
		&animationObjects_,
		debugAnimationPreview_.GetHandWeapon(),
		skyBox_.get(),
		&sprites_,
	});
	// Draw完了後のRenderTextureを使い、UI自動確認の画像・動画保存を進めます。
	DebugUiSmoke::UpdateAfterDraw(uiSmoke_, MakeUiSmokeContext());
}
void DebugScene::Finalize()
{
	//GPUの完了を待機
	DirectXCommon::GetInstance()->WaitForGPU();

	// 中途半端に生き残っている粒子が原因のアクセス違反を防ぐ
	ParticleManager::GetInstance()->SetCameraManager(nullptr);
	ParticleManager::GetInstance()->ClearAllGroups();

	activeEmitter_ = nullptr;
	objectPlane_ = nullptr;
	gameViewCapture_.Finalize();
	debugSceneEditor_.Finalize();
	levelRuntime_.Reset();
	baseAnimationObjectCount_ = 0;
	baseSpriteCount_ = 0;
	selection_.ClearAll();
	assetPreview_.Clear();
	gameViewCameraController_.Reset();
	debugAnimationPreview_.Finalize();
	emitterCircle_.reset();
	emitterPlane_.reset();
	skyBox_.reset();
	sprites_.clear();
	normalObjects_.clear();
	animationObjects_.clear();
	entityRegistry_.Clear();
	upCamera_.reset();
	mainCamera.reset();
	cameraManager.reset();
}

