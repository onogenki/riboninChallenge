#include "Stage1.h"

#include "Camera.h"
#include "CameraController.h"
#include "Audio.h"
#include "CarryableMirror.h"
#include "DirectXCommon.h"
#include "Collision.h"
#include "FixedMirror.h"
#include "Input.h"
#include "ImGuiManager.h"
#include "LevelLoader.h"
#include "Object3dCommon.h"
#include "Object3dFactory.h"
#include "Object3dRenderContext.h"
#include "Player.h"
#include "PostEffect.h"
#include "SceneRenderPipeline.h"
#include "SceneManager.h"
#include "SrvManager.h"
#include "StageMapObjectIndex.h"
#include "MapChipRegistry.h"
#include "StageMapChipFactory.h"
#include "StageMirrorFactory.h"
#include "StageReflectionRenderer.h"
#include "StageSceneRenderer.h"
#include "debug/StagePuzzleDebugUi.h"
#include <algorithm>
#include <cmath>
#include <dinput.h>
#include <functional>

using namespace MyMath;

namespace
{
	constexpr const char* kStageMapFilePath = "resources/levels/stage1.json";
	constexpr const char* kStageMapFileName = "stage1";
	constexpr const char* kStageMapChipFilePath = "resources/maps/stage1.csv";
	// CSVの一マスを、ゲーム内でも1x1x1のブロックとして配置します。
	constexpr float kStageMapChipCellSize = 1.0f;
	// P0が従来と同じX=24、Z=35になるよう、CSV左上の座標を決めます。
	const Vector3 kStageMapChipOrigin{ 22.0f, 0.0f, 33.0f };
	// 高さ1.0のブロック上面をY=-2.0へ揃える中心高さです。
	constexpr float kStageMapChipFloorY = -2.5f;
	// Playerの球Colliderが床上面へ接する開始高さです。
	constexpr float kStageMapChipPlayerY = -0.8f;
}

// 前方宣言したunique_ptrの型を、この実装ファイルで生成できるようにします。
Stage1::Stage1() = default;

// Stage1が所有するPlayer・Mirror・Rendererを、完全な型を知る場所で破棄します。
Stage1::~Stage1() = default;

void Stage1::Initialize()
{
	hasStageFrameBeenDrawn_ = false;
	hasStageBgmStarted_ = false;
	InitializeRenderSystems();
	// BGMの再生役だけを準備し、音声ファイルの読込はStage1画面の表示後まで遅らせます。
	stageBgm_.Initialize(Audio::GetInstance());
	InitializeDefaultLighting();
	if (!InitializeStageGimmicks()) {
		return;
	}
	// JSONが読めない時に予備座標の鏡を表示しないよう、Stage開始をここで止めます。
	if (!InitializeStageMap()) {
		return;
	}
	if (!InitializePlayerAndCamera()) {
		return;
	}
	InitializeGameplaySmoke();
}

// DirectX・Camera・Object3dを、Stage1が使える初期状態へそろえます。
void Stage1::InitializeRenderSystems()
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	lightPuzzle_.Initialize(dxCommon);
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);

	// Playerが部屋を見る通常Cameraを、まず作成します。
	InitializeMainCamera({ 0.0f, 1.0f, -12.0f });
	object3dCommon = Object3dCommon::GetInstance();
	object3dCommon->Initialize(dxCommon);
	object3dCommon->SetDefaultCamera(cameraManager->GetActiveCamera());
	sceneObjects_.Initialize(object3dCommon);
}

// JSONにLightingがない場合にも使える、Stage1の初期照明を設定します。
void Stage1::InitializeDefaultLighting()
{
	directionalLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLight_.direction = Normalize({ 0.5f, -1.0f, 0.5f });
	directionalLight_.intensity = 0.3f;
	directionalLight_.ambientColor = { 1.0f, 1.0f, 1.0f };
	directionalLight_.ambientIntensity = 0.0f;
	pointLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLight_.position = { 0.0f, 3.0f, -2.0f };
	pointLight_.intensity = 5.0f;
	pointLight_.radius = 20.0f;
	pointLight_.decay = 1.0f;
}

// CollectionとFactoryへ生成を依頼し、必須の床・携帯鏡・Puzzle部品がそろったか確認します。
bool Stage1::InitializeStageGimmicks()
{
	StageLightPuzzle::Settings& puzzleSettings = lightPuzzle_.GetSettings();
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// 床はJSON読込後に配置を上書きするため、ここでは読込失敗時用の初期値を置きます。
	floor_ = sceneObjects_.Create("debug/floor.obj");
	if (!floor_) {
		return false;
	}
	floor_->SetScale({ 1.0f, 1.0f, 1.0f });
	floor_->SetRotate({ 0.0f, 0.0f, 0.0f });
	// floor.objの高さは3.0なので、上面がY=-2.0になる中心位置に置きます。
	floor_->SetTranslate({ 0.0f, -3.500001f, 5.0f });

	// 実際の配置は後でstage1.jsonから反映します。Factoryは所持状態へ切り替えません。
	carryableMirror_ = StageMirrorFactory::CreateCarryableMirror(
		object3dCommon,
		// 以前から携帯Mirrorに使っていた白い板Modelを使います。
		"debug/plane.obj",
		{ -2.5f, -0.8f, 4.5f },
		3.6f,
		3.6f);
	if (!carryableMirror_) {
		// 持てるMirrorが作れないStageは、見えないまま続行させません。
		return false;
	}

	// 鏡床は従来通り任意の部品です。生成失敗ならnullptrのまま、ほかの部品を準備します。
	mirrorFloor_ = StageMirrorFactory::CreateMirrorFloor(
		object3dCommon,
		"debug/plane.obj",
		mirrorFloorPosition_,
		mirrorFloorWidth_,
		mirrorFloorHeight_);

	// 白い小球をLaserの発射装置として置き、光がどこから出るかを見えるようにします。
	laserEmitter_ = sceneObjects_.Create("debug/sphere.obj");
	if (!laserEmitter_) {
		return false;
	}
	laserEmitter_->SetTranslate(puzzleSettings.laserOrigin);
	laserEmitter_->SetScale({ 0.9f, 0.9f, 0.9f });
	laserEmitter_->SetTextureOverride("resources/debug/white.png");

	doorLaserEmitter_ = sceneObjects_.Create("debug/sphere.obj");
	if (!doorLaserEmitter_) {
		return false;
	}
	doorLaserEmitter_->SetTranslate(puzzleSettings.doorLaserOrigin);
	doorLaserEmitter_->SetScale({ 0.65f, 0.65f, 0.65f });
	doorLaserEmitter_->SetTextureOverride("resources/debug/white.png");

	// 危険Lightの色・太さ・描画用Rendererは、軌道と同じギミック部品が所有します。
	hazardLights_.Initialize(dxCommon);

	// 二つのSwitchはSphere、Doorは厚みのあるfloor.objを縮小して表現します。
	chargeSwitch_ = sceneObjects_.Create("debug/sphere.obj");
	if (!chargeSwitch_) {
		return false;
	}
	chargeSwitch_->SetTranslate(puzzleSettings.chargeSwitchPosition);
	chargeSwitch_->SetScale({
		puzzleSettings.chargeSwitchRadius * 2.0f,
		puzzleSettings.chargeSwitchRadius * 2.0f,
		puzzleSettings.chargeSwitchRadius * 2.0f,
	});
	chargeSwitch_->SetTextureOverride("resources/debug/white.png");

	doorSwitch_ = sceneObjects_.Create("debug/sphere.obj");
	if (!doorSwitch_) {
		return false;
	}
	doorSwitch_->SetTranslate(puzzleSettings.doorSwitchPosition);
	doorSwitch_->SetScale({
		puzzleSettings.doorSwitchRadius * 2.0f,
		puzzleSettings.doorSwitchRadius * 2.0f,
		puzzleSettings.doorSwitchRadius * 2.0f,
	});
	doorSwitch_->SetTextureOverride("resources/debug/white.png");

	lightDoor_ = sceneObjects_.Create("debug/floor.obj");
	if (!lightDoor_) {
		return false;
	}
	lightDoor_->SetTranslate(puzzleSettings.doorClosedPosition);
	lightDoor_->SetScale({ 0.20f, 1.00f, 0.05f });
	return true;
}

// Stage1用JSONとCSVを読み、ColliderとHot Reload監視を開始します。
bool Stage1::InitializeStageMap()
{
	stageMapHotReload_.SetFilePath(kStageMapFilePath);
	stageMapChipHotReload_.SetFilePath(kStageMapChipFilePath);
	if (!ReloadStageMap()) {
		return false;
	}
	// 最初のPlayer更新より前に、閉じたDoorのColliderとLaser経路を作ります。
	UpdateLightPuzzle(0.0f);
	collisionWorld_.Rebuild({
		floor_,
		&fixedMirrors_,
		carryableMirror_.get(),
		&lightPuzzle_,
		&stageMapRuntime_,
		lightDoor_ != nullptr,
	});
	stageMapHotReload_.Synchronize();
	stageMapChipHotReload_.Synchronize();
	return true;
}

// Player、通常追従Camera、開始演出を、この順番で作成します。
bool Stage1::InitializePlayerAndCamera()
{
	const Vector3 playerStartPosition = hasStagePlayerStart_
		? stagePlayerStartPosition_
		: Vector3{ 0.0f, -0.8f, 5.0f };
	player_ = std::make_unique<Player>();
	if (!player_->Initialize(object3dCommon, "debug/sphere.obj", playerStartPosition, 1.2f)) {
		// Playerモデルを作れない状態で、Cameraや開始演出がnullptrを参照しないよう中断します。
		player_.reset();
		return false;
	}
	// sphere.obj本来のUVチェッカーを使わず、Playerだけへモンスターボール画像を明示します。
	player_->GetObject().SetTextureOverride("resources/debug/monsterBall.png");

	// Camera本体とは別のControllerに、Playerを追従する規則を任せます。
	cameraController_ = std::make_unique<CameraController>();
	cameraController_->Initialize(mainCamera.get(), player_->GetPosition());
	cameraController_->SetAutoRecenterEnabled(false);
	// 建物の外壁だけは越えないようにし、室内壁の向こうはシルエット表示を優先します。
	cameraController_->SetWallAvoidanceEnabled(true);

	// 通常Cameraの完成位置を保存してから、開始演出用の前上方Cameraへ切り替えます。
	stageStart_ = std::make_unique<StageStart>();
	stageStart_->Begin(
		*player_,
		*mainCamera,
		playerStartPosition,
		mainCamera->GetTranslate(),
		mainCamera->GetRotate(),
		stageStartSettings_);
	stageEditor_.Initialize();
	return true;
}

// 自動動作確認に必要なStage固有データだけを渡します。
void Stage1::InitializeGameplaySmoke()
{
	StageLightPuzzle::Settings& puzzleSettings = lightPuzzle_.GetSettings();
	gameplaySmoke_.Initialize({
		player_.get(),
		carryableMirror_.get(),
		&fixedMirrors_,
		mirrorFloor_.get(),
		puzzleSettings.laserOrigin,
		puzzleSettings.laserDirection,
		puzzleSettings.laserCollisionRadius,
		puzzleSettings.doorLaserOrigin,
		puzzleSettings.doorLaserDirection,
		puzzleSettings.doorSwitchPosition,
		puzzleSettings.doorSwitchRadius,
		puzzleSettings.largeMirrorBaseYaw,
		puzzleSettings.largeMirrorTargetYawOffset,
		object3dCommon,
	});
}

void Stage1::Finalize()
{
	// unique_ptr がオブジェクトを自動的に解放します。
	// SceneManager がこの関数の前に GPU の処理完了を待機します。
	stageCameraEvents_.Clear(cameraManager.get());
	stageEditor_.Finalize();
	sceneObjects_.Clear();
	stageMapRuntime_.Clear();
	collisionWorld_.Clear();
	stageMapData_.reset();
	stageMapChipField_ = {};
	floor_ = nullptr;
	laserEmitter_ = nullptr;
	doorLaserEmitter_ = nullptr;
	chargeSwitch_ = nullptr;
	doorSwitch_ = nullptr;
	lightDoor_ = nullptr;
	fixedMirrors_.clear();
	mirrorFloor_.reset();
	carryableMirror_.reset();
	lightPuzzle_.Finalize();
	stageStart_.reset();
	stageBgm_.Stop();
	cameraController_.reset();
	player_.reset();
}

void Stage1::Update()
{
	// ---------- Stage1マップのホットリロード ----------
	UpdateStageMapHotReload();
	if (ImGuiManager::GetInstance()->IsGameViewActive()) {
		stageMapRuntime_.UpdatePaths(DirectXCommon::GetInstance()->GetDeltaTime());
	}

	// ---------- プレイヤーの移動と重力 ----------
	const float deltaTime = DirectXCommon::GetInstance()->GetDeltaTime();
	const bool isStageStartPlaying = UpdateStagePlayer(deltaTime);
	UpdateStageBgm(deltaTime);
	// 本編のDoor・危険Lightの結果だけを、自動確認部品へ渡します。
	gameplaySmoke_.Update(
		{
			&hazardLights_,
			&collisionWorld_.GetSolidObbs(),
			lightPuzzle_.IsDoorSwitchReceivingLight(),
			lightPuzzle_.GetDoorOpenAmount(),
		},
		deltaTime);
	UpdateMirrorGameplay();
	UpdateLightPuzzle(deltaTime);
	// 危険Lightの軌道・反射・Player接触は、Stage固有ギミックへまとめて任せます。
	hazardLights_.Update(
		deltaTime,
		player_ ? &player_->GetSphere() : nullptr,
		[this](const std::vector<LaserSegment>& sourceSegments)
		{
			return ReflectHazardLightSegments(sourceSegments);
		});
	// 見た目だけの線ではなく、反射後の経路も周囲を照らすSpotLightへ反映します。
	UpdateLaserSpotLights();

	// ---------- カメラとデバッグ UI の更新 ----------
	UpdateStageCamera(deltaTime, isStageStartPlaying);
	UpdateReflectionCameras();
	ImGuiManager::GetInstance()->Begin("Stage1");
	// Gameplay中もPlayerの球Colliderを表示し、Laser接触を黄色で確認できるようにする
	if (player_) {
		ImGuiManager::GetInstance()->DrawPlayerCollisionDebug(
			player_->GetSphere(),
			cameraManager->GetActiveCamera(),
			player_->IsColliding(),
			lightPuzzle_.IsPlayerHitByLaser() || hazardLights_.IsPlayerHit());
	}
	DrawStagePuzzleDebugUi();
	DrawStageEditorUi();
	ImGuiManager::GetInstance()->End();

	// ---------- 3D オブジェクトの描画準備 ----------
	// Stage1全体で同じCamera・Lightを一回だけまとめ、全Objectへ適用します。
	Object3dRenderContext renderContext(
		cameraManager ? cameraManager->GetActiveCamera() : nullptr,
		directionalLight_,
		pointLight_,
		spotLights_);
	renderContext.UpdateObjects(sceneObjects_.GetObjects());
	stageMapRuntime_.UpdateRenderObjects(renderContext);
	for (const auto& fixedMirror : fixedMirrors_) {
		if (fixedMirror) {
			renderContext.UpdateObject(fixedMirror->GetObject());
		}
	}
	if (mirrorFloor_) {
		renderContext.UpdateObject(mirrorFloor_->GetObject());
	}
	if (carryableMirror_) {
		renderContext.UpdateObject(carryableMirror_->GetObject());
	}
	if (player_) {
		renderContext.UpdateObject(player_->GetObject());
	}
	// Game ViewのBackSpaceだけで戻り、Stage1の解放中は共通Loading画面を表示します。
	if (ImGuiManager::GetInstance()->IsGameViewActive() &&
		Input::GetInstance()->TriggerKey(DIK_BACK)) {
		SceneManager::GetInstance()->ChangeSceneWithLoading("TITLE");
	}
}

void Stage1::Draw()
{
	// ---------- 固定鏡の反射Textureを先に作成 ----------
	SrvManager::GetInstance()->PreDraw();
	StageReflectionRenderer::Context reflectionContext{};
	reflectionContext.directXCommon = DirectXCommon::GetInstance();
	reflectionContext.object3dCommon = object3dCommon;
	reflectionContext.activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	reflectionContext.sceneObjects = &sceneObjects_.GetObjects();
	reflectionContext.stageMapRuntime = &stageMapRuntime_;
	reflectionContext.fixedMirrors = &fixedMirrors_;
	reflectionContext.mirrorFloor = mirrorFloor_.get();
	reflectionContext.carryableMirror = carryableMirror_.get();
	reflectionContext.player = player_.get();
	reflectionContext.lightPuzzle = &lightPuzzle_;
	reflectionContext.hazardLights = &hazardLights_;
	StageReflectionRenderer::DrawOne(reflectionContext, reflectionUpdateCursor_);

	// ---------- ゲーム画面への描画 ----------
	DirectXCommon::GetInstance()->PreDraw();
	StageSceneRenderer::Context sceneRendererContext{};
	sceneRendererContext.object3dCommon = object3dCommon;
	sceneRendererContext.activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	sceneRendererContext.sceneObjects = &sceneObjects_.GetObjects();
	sceneRendererContext.stageMapRuntime = &stageMapRuntime_;
	sceneRendererContext.fixedMirrors = &fixedMirrors_;
	sceneRendererContext.mirrorFloor = mirrorFloor_.get();
	sceneRendererContext.carryableMirror = carryableMirror_.get();
	sceneRendererContext.player = player_.get();
	sceneRendererContext.lightPuzzle = &lightPuzzle_;
	sceneRendererContext.hazardLights = &hazardLights_;
	StageSceneRenderer::Draw(sceneRendererContext);

	// PostEffect・SwapChain・ImGui・Presentは、全Scene共通のPipelineへ任せます。
	SceneRenderPipeline::End(
		DirectXCommon::GetInstance(),
		cameraManager ? cameraManager->GetActiveCamera() : nullptr);
	// 次のUpdateからBGMを読み込めるよう、最初の本編画面が出たことを記録します。
	hasStageFrameBeenDrawn_ = true;
}

std::unique_ptr<Object3d> Stage1::CreateRuntimeObject(const std::string& modelName)
{
	// 生成時はモデル読込とObject3d初期化だけを行い、Camera・LightはUpdateで一括設定します。
	return Object3dFactory::Create(object3dCommon, modelName);
}

// 開始演出・自動確認・通常入力のいずれかでPlayerを更新し、開始演出中ならtrueを返します。
bool Stage1::UpdateStagePlayer(float deltaTime)
{
	if (!player_ || !floor_) {
		return false;
	}

	collisionWorld_.Rebuild({
		floor_,
		&fixedMirrors_,
		carryableMirror_.get(),
		&lightPuzzle_,
		&stageMapRuntime_,
		lightDoor_ != nullptr,
	});
	const bool isGameViewActive = ImGuiManager::GetInstance()->IsGameViewActive();
	const bool isStageStartPlaying =
		isGameViewActive &&
		stageStart_ &&
		stageStart_->Update(deltaTime, *player_, *mainCamera);
	if (isStageStartPlaying) {
		// 開始演出中はPlayer入力・Jump・Mirror操作を受け付けません。
		return true;
	}

	if (gameplaySmoke_.IsEnabled()) {
		// 自動検証ではCameraに影響されない世界+X方向へ歩かせます。
		Player::ControlInput smokeControl{};
		smokeControl.right = 1.0f;
		player_->UpdateWithControl(
			deltaTime,
			collisionWorld_.GetSolidObbs(),
			{ 0.0f, 0.0f, 1.0f },
			smokeControl);
		return false;
	}

	if (!isGameViewActive) {
		return false;
	}

	// Mouse左右クリックで携帯Mirrorを構えている間は、Playerの移動・Jumpを弱めます。
	const bool isMirrorGuarding =
		carryableMirror_ &&
		carryableMirror_->IsCarried() &&
		(Input::GetInstance()->IsMouseButtonPressed(0) ||
			Input::GetInstance()->IsMouseButtonPressed(1));
	player_->SetMirrorGuardMode(isMirrorGuarding);
	// 現在画面に映しているCameraの正面を渡し、WASDを画面基準の移動へ変換します。
	Vector3 cameraForward{ 0.0f, 0.0f, 1.0f };
	if (cameraManager) {
		if (Camera* activeCamera = cameraManager->GetActiveCamera()) {
			cameraForward = GetCameraForward(*activeCamera);
		}
	}
	player_->Update(deltaTime, collisionWorld_.GetSolidObbs(), cameraForward);
	return false;
}

// 固定鏡の位置・向きから、各鏡面用Cameraの反射行列を更新します。
void Stage1::UpdateReflectionCameras()
{
	Camera* sourceCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	if (!sourceCamera) {
		return;
	}

	const Vector3 sourceForward = GetCameraForward(*sourceCamera);
	for (const auto& fixedMirror : fixedMirrors_) {
		if (fixedMirror) {
			fixedMirror->UpdateReflectionCamera(*sourceCamera, sourceForward);
		}
	}
	if (mirrorFloor_) {
		mirrorFloor_->UpdateReflectionCamera(*sourceCamera, sourceForward);
	}
}

// 鏡の状態を受け取り、Laser・Switch・DoorのPuzzle規則を専用部品へ任せます。
void Stage1::UpdateLightPuzzle(float deltaTime)
{
	if (!chargeSwitch_ || !doorSwitch_ || !lightDoor_ ||
		fixedMirrors_.empty() || !fixedMirrors_.front()) {
		return;
	}
	const std::vector<const Mirror*> reflectors = GetLaserReflectors();
	lightPuzzle_.Update(
		deltaTime,
		reflectors,
		collisionWorld_.GetLightBlockingObbs(),
		player_ ? &player_->GetSphere() : nullptr,
		fixedMirrors_.front().get(),
		laserEmitter_,
		doorLaserEmitter_,
		chargeSwitch_,
		doorSwitch_,
		lightDoor_);
}

// 開始演出・通常追従・Event Cameraの優先順位で、Stage1のCameraを更新します。
void Stage1::UpdateStageCamera(float deltaTime, bool isStageStartPlaying)
{
	if (!cameraManager) {
		return;
	}

	if (ImGuiManager::GetInstance()->IsGameViewActive() && !isStageStartPlaying) {
		UpdateMainCamera();
		if (stageStart_ && mainCamera && stageStart_->IsCameraHandoffPlaying()) {
			// 通常Cameraの壁回避・追従結果へ、開始演出Cameraを滑らかに近づけます。
			stageStart_->UpdateCameraHandoff(deltaTime, *mainCamera);
		}
		if (player_) {
			stageCameraEvents_.UpdateEventCamera(
				*player_,
				*cameraManager,
				collisionWorld_.GetCameraBoundaryObbs(),
				deltaTime);
			if (const std::string status = stageCameraEvents_.ConsumeStatus(); !status.empty()) {
				stageMapReloadStatus_ = status;
			}
		}
	} else if (!stageCameraEvents_.GetActiveEventCameraName().empty()) {
		// Edit Viewではイベントカメラへ自動切替せず、編集用のMainCameraを維持します。
		stageCameraEvents_.RestoreMainCamera(*cameraManager);
	}
	cameraManager->Update();
}

// 最初のStage1描画を待ってからBGMを読み込むため、音が白いLoading画面より先に流れません。
void Stage1::UpdateStageBgm(float deltaTime)
{
	if (!hasStageFrameBeenDrawn_) {
		return;
	}

	if (!hasStageBgmStarted_) {
		// 現在ある短い音源を動作確認用にループします。曲を追加したらこのパスだけ差し替えます。
		hasStageBgmStarted_ = stageBgm_.Play("resources/debug/Alarm01.wav", 0.20f, 0.80f);
	}
	stageBgm_.Update(deltaTime);
}

// Player位置・向き・壁回避の結果から、通常追従Cameraを更新します。
void Stage1::UpdateMainCamera()
{
	if (!player_ || !cameraController_) {
		return;
	}
	stageCameraEvents_.UpdateCameraArea(*player_, *cameraController_);
	// 携帯Mirrorを持っている間は、構え操作でCameraの通常距離が近付かないようにします。
	// 壁がCameraとPlayerの間にある時だけは、CameraControllerの壁回避が優先されます。
	cameraController_->SetDistanceLock(carryableMirror_ && carryableMirror_->IsCarried());
	if (const std::string status = stageCameraEvents_.ConsumeStatus(); !status.empty()) {
		stageMapReloadStatus_ = status;
	}

	// 通常時のCameraは、矢印キー一回につき一段階だけ位置を変更する
	Input* input = Input::GetInstance();
	bool isCameraStepInput = false;
	if (input->TriggerKey(DIK_LEFT)) {
		isCameraStepInput |= cameraController_->TryStepOrbit(1, collisionWorld_.GetCameraBoundaryObbs());
	} else if (input->TriggerKey(DIK_RIGHT)) {
		isCameraStepInput |= cameraController_->TryStepOrbit(-1, collisionWorld_.GetCameraBoundaryObbs());
	}
	if (input->TriggerKey(DIK_UP)) {
		isCameraStepInput |= cameraController_->TryStepDistance(1, collisionWorld_.GetCameraBoundaryObbs());
	} else if (input->TriggerKey(DIK_DOWN)) {
		isCameraStepInput |= cameraController_->TryStepDistance(-1, collisionWorld_.GetCameraBoundaryObbs());
	}

	// Rを押すと、Playerが最後に向いた方向の後ろへCameraをゆっくり戻す
	if (input->TriggerKey(DIK_R)) {
		cameraController_->ResetBehindTarget(player_->GetFacingYaw());
	}

	// Playerの進行方向だけを渡し、先読みと自動リセンターに使用する
	cameraController_->Update(
		DirectXCommon::GetInstance()->GetDeltaTime(),
		player_->GetPosition(),
		player_->GetMoveDirection(),
		isCameraStepInput,
		collisionWorld_.GetCameraBoundaryObbs());
}

// Cameraのワールド行列から、Player移動に使う正面方向を返します。
Vector3 Stage1::GetCameraForward(const Camera& camera) const
{
	// このプロジェクトでは、カメラのローカル座標 +Z が正面です。
	const Matrix4x4& worldMatrix = camera.GetWorldMatrix();
	return Normalize({ worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2] });
}

// 読込済みLevelDataの照明設定を、Stage1全体で共有するLightへ反映します。
void Stage1::ApplyStageLighting()
{
	stageSpotLights_.clear();
	if (!stageMapData_ || !stageMapData_->hasLighting) {
		return;
	}

	const LevelLoader::LightingData& lighting = stageMapData_->lighting;
	directionalLight_.color = {
		lighting.directionalColor.x,
		lighting.directionalColor.y,
		lighting.directionalColor.z,
		1.0f,
	};
	directionalLight_.direction = Normalize(lighting.directionalDirection);
	directionalLight_.intensity = lighting.directionalIntensity;
	directionalLight_.ambientColor = lighting.ambientColor;
	directionalLight_.ambientIntensity = lighting.ambientIntensity;
	pointLight_.color = {
		lighting.pointColor.x,
		lighting.pointColor.y,
		lighting.pointColor.z,
		1.0f,
	};
	pointLight_.position = lighting.pointPosition;
	pointLight_.intensity = lighting.pointIntensity;
	pointLight_.radius = lighting.pointRadius;
	pointLight_.decay = lighting.pointDecay;
	for (const LevelLoader::SpotLightData& spotLightData : lighting.spotLights) {
		if (stageSpotLights_.size() >= kStageLightingSpotLightCount) {
			break;
		}
		if (spotLightData.intensity <= 0.0f || spotLightData.distance <= 0.0f) {
			continue;
		}

		Object3d::SpotLight spotLight{};
		spotLight.color = {
			spotLightData.color.x,
			spotLightData.color.y,
			spotLightData.color.z,
			1.0f,
		};
		spotLight.position = spotLightData.position;
		spotLight.intensity = spotLightData.intensity;
		spotLight.direction = Normalize(spotLightData.direction);
		spotLight.distance = spotLightData.distance;
		spotLight.decay = spotLightData.decay;
		spotLight.cosAngle = spotLightData.cosAngle;
		spotLight.cosFalloffStart = spotLightData.cosFalloffStart;
		stageSpotLights_.push_back(spotLight);
	}
}

// Player開始位置、持てるMirror位置、StageStart演出設定を読込済みLevelDataから反映します。
void Stage1::ApplyStageStartSettings(
	const LevelLoader::ObjectData* playerStartData,
	const LevelLoader::ObjectData* carryableMirrorData,
	const MapChipField::Chip* csvPlayerStartChip)
{
	// P0がCSVにあればJSONのPlayerStartより優先します。
	// Hot Reload時にはPlayerを移動させないため、実際の配置はInitializeで一度だけ行います。
	hasStagePlayerStart_ = csvPlayerStartChip != nullptr || playerStartData != nullptr;
	if (csvPlayerStartChip) {
		stagePlayerStartPosition_ = stageMapChipField_.GetPosition(
			*csvPlayerStartChip,
			kStageMapChipOrigin,
			kStageMapChipCellSize);
		stagePlayerStartPosition_.y = kStageMapChipPlayerY;
	} else if (playerStartData) {
		stagePlayerStartPosition_ = playerStartData->translation;
	}

	if (carryableMirrorData && carryableMirror_) {
		// 持てるMirrorの置き場所は、ほかのStage物と同じくJSONで変更します。
		carryableMirror_->SetDroppedPosition(carryableMirrorData->translation);
	}
	if (stageMapData_ && stageMapData_->hasStageStart) {
		stageStartSettings_.duration = stageMapData_->stageStart.duration;
		stageStartSettings_.playerAirHeight = stageMapData_->stageStart.playerAirHeight;
		stageStartSettings_.cameraFrontDistance = stageMapData_->stageStart.cameraFrontDistance;
		stageStartSettings_.cameraFrontHeight = stageMapData_->stageStart.cameraFrontHeight;
		stageStartSettings_.cameraOrbitAngle = stageMapData_->stageStart.cameraOrbitAngle;
		stageStartSettings_.cameraHandoffDuration = stageMapData_->stageStart.cameraHandoffDuration;
	}
}

// モデルの読込失敗時に旧配置を残すため、通常3D配置物を仮Runtimeへ作ります。
bool Stage1::BuildStageRuntime(
	const std::vector<const LevelLoader::ObjectData*>& additionalObjects,
	StageMapRuntime& outRuntime)
{
	return outRuntime.Rebuild(
		additionalObjects,
		[this](const std::string& modelName)
		{
			return CreateRuntimeObject(modelName);
		});
}

// Editorで変更した通常3D配置物とCamera Eventを、再生成せず反映します。
void Stage1::ApplyStageRuntimeEdits(
	const std::vector<const LevelLoader::ObjectData*>& additionalObjects,
	const std::vector<const LevelLoader::ObjectData*>& eventTriggerDataList,
	const std::vector<const LevelLoader::ObjectData*>& eventCameraDataList,
	const std::vector<const LevelLoader::ObjectData*>& cameraAreaDataList)
{
	stageMapRuntime_.ApplyEdits(additionalObjects);
	stageCameraEvents_.ApplyEdits(
		eventTriggerDataList,
		eventCameraDataList,
		cameraAreaDataList);
}

// JSONの床TransformとBOX Colliderを、Stage1が所有する床へ反映します。
void Stage1::ApplyFloorData(const LevelLoader::ObjectData& floorData)
{
	if (!floor_) {
		return;
	}

	floor_->SetTranslate(floorData.translation);
	floor_->SetRotate(floorData.rotation);
	floor_->SetScale(floorData.scaling);
	if (floorData.hasCollider && floorData.collider.type == "BOX") {
		collisionWorld_.SetFloorLocalShape(
			floorData.collider.center,
			{
				std::abs(floorData.collider.size.x) * 0.5f,
				std::abs(floorData.collider.size.y) * 0.5f,
				std::abs(floorData.collider.size.z) * 0.5f,
			});
	}
}

// 読込済みJSON/CSVの必要なモデルを先に仮生成し、成功後に床・鏡・Cameraへ反映します。
bool Stage1::ApplyStageMapData(bool rebuildRuntimeObjects)
{
	// JSONのObjectDataを分類し、既存の床・鏡・Camera・追加モデルへ反映する中心処理です。
	if (!stageMapData_ || !floor_) {
		return false;
	}

	// JSONのタグとobject_typeの分類は、SceneではなくLevel用のIndexへ任せます。
	StageMapObjectIndex objectIndex;
	objectIndex.Build(*stageMapData_);
	const LevelLoader::ObjectData* floorData = objectIndex.GetFloor();
	const LevelLoader::ObjectData* playerStartData = objectIndex.GetPlayerStart();
	const LevelLoader::ObjectData* carryableMirrorData = objectIndex.GetCarryableMirror();
	const std::vector<const LevelLoader::ObjectData*>& mirrorDataList = objectIndex.GetFixedMirrors();
	std::vector<const LevelLoader::ObjectData*> additionalObjects = objectIndex.GetRuntimeObjects();
	const std::vector<const LevelLoader::ObjectData*>& eventTriggerDataList = objectIndex.GetEventTriggers();
	const std::vector<const LevelLoader::ObjectData*>& eventCameraDataList = objectIndex.GetEventCameras();
	const std::vector<const LevelLoader::ObjectData*>& cameraAreaDataList = objectIndex.GetCameraAreas();
	std::vector<LevelLoader::ObjectData> mapChipObjectDataList;

	// Stage1が使う番号だけを登録します。追加時は種類・番号と処理をここへ一組増やします。
	// E0・G0・C0・L0は未登録なので、今は表示も当たり判定も持ちません。
	MapChipRegistry chipRegistry;
	const MapChipField::Chip* csvPlayerStartChip = nullptr;
	chipRegistry.Register(MapChipType::PlayerStart, 0,
		[&csvPlayerStartChip](const MapChipField::Chip& chip)
		{
			if (!csvPlayerStartChip) {
				csvPlayerStartChip = &chip;
			}
		});
	chipRegistry.Register(MapChipType::Block, 0,
		[this, &mapChipObjectDataList](const MapChipField::Chip& chip)
		{
			mapChipObjectDataList.push_back(StageMapChipFactory::CreateBlockData(
				stageMapChipField_, chip, kStageMapChipOrigin,
				kStageMapChipCellSize, kStageMapChipFloorY));
		});
	for (const MapChipField::Chip& chip : stageMapChipField_.GetChips()) {
		chipRegistry.Run(chip);
	}
	for (const LevelLoader::ObjectData& blockData : mapChipObjectDataList) {
		additionalObjects.push_back(&blockData);
	}

	// Stage1で必須の床と鏡がなければ、途中まで生成した状態を残さず何も変更しない
	if (!floorData || mirrorDataList.empty()) {
		return false;
	}

	// Mirrorタグの数だけ固定鏡を作るため、JSONへMirrorを追加すれば複数配置できます。
	const bool rebuildFixedMirrors =
		rebuildRuntimeObjects || fixedMirrors_.size() != mirrorDataList.size();
	std::vector<std::unique_ptr<FixedMirror>> rebuiltFixedMirrors;
	if (rebuildFixedMirrors) {
		if (!StageMirrorFactory::CreateFixedMirrors(object3dCommon, mirrorDataList, rebuiltFixedMirrors)) {
			return false;
		}
	} else {
		// 編集反映の途中でnullptrに当たり、床などだけ新しくならないよう先に確認します。
		for (size_t index = 0; index < fixedMirrors_.size(); ++index) {
			if (!fixedMirrors_[index] || !mirrorDataList[index]) {
				return false;
			}
		}
	}

	StageMapRuntime rebuiltMapRuntime;
	if (rebuildRuntimeObjects && !BuildStageRuntime(additionalObjects, rebuiltMapRuntime)) {
		return false;
	}

	// 失敗し得る生成が終わってから、Sceneが所有する状態をまとめて更新します。
	ApplyStageStartSettings(playerStartData, carryableMirrorData, csvPlayerStartChip);
	ApplyStageLighting();
	ApplyFloorData(*floorData);

	// ---------- 鏡データの反映 ----------
	if (rebuildFixedMirrors) {
		fixedMirrors_ = std::move(rebuiltFixedMirrors);
	} else if (!StageMirrorFactory::ApplyFixedMirrorEdits(mirrorDataList, fixedMirrors_)) {
		return false;
	}
	// Inspector・JSONで変えた基準角度をPuzzleにも渡し、次の更新で元へ戻るのを防ぎます。
	// プレイ中の充電による回転量は保持し、Levelデータとは分けて扱います。
	StageLightPuzzle::Settings& puzzleSettings = lightPuzzle_.GetSettings();
	puzzleSettings.largeMirrorBaseYaw = mirrorDataList.front()->rotation.y;
	fixedMirrors_.front()->GetYawForEdit() = puzzleSettings.largeMirrorBaseYaw +
		puzzleSettings.largeMirrorTargetYawOffset * lightPuzzle_.GetLargeMirrorRotationAmountForEdit();
	fixedMirrors_.front()->SyncVisualAndCollider();

	if (rebuildRuntimeObjects) {
		stageMapRuntime_ = std::move(rebuiltMapRuntime);
		stageCameraEvents_.Rebuild(
			eventTriggerDataList,
			eventCameraDataList,
			cameraAreaDataList,
			cameraManager.get(),
			cameraController_.get());
	} else {
		ApplyStageRuntimeEdits(
			additionalObjects,
			eventTriggerDataList,
			eventCameraDataList,
			cameraAreaDataList);
	}

	return true;
}
