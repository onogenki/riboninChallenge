#include "Stage.h"

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
#include "Logger.h"
#include "Object3dCommon.h"
#include "Object3dFactory.h"
#include "Player.h"
#include "PostEffect.h"
#include "SceneRenderPipeline.h"
#include "SceneManager.h"
#include "SkyBox.h"
#include "SpriteCommon.h"
#include "SrvManager.h"
#include "StageMapObjectIndex.h"
#include "StageMapChipFactory.h"
#include "StageGimmickFactory.h"
#include "StageMirrorFactory.h"
#include "render/StageReflectionRenderer.h"
#include "render/StageRenderContext.h"
#include "render/StageSceneRenderer.h"
#include "StageTitleOverlay.h"
#include "StagePauseOverlay.h"
#include "../gameOver/GameOverScene.h"
#include "debug/StagePuzzleDebugUi.h"
#include <cmath>
#include <dinput.h>
#include <functional>
#include <numbers>
#include <Windows.h>

using namespace MyMath;

namespace
{
	constexpr const char* kSkyBoxTexturePath = "Resources/backGround/skybox-night-cube.dds";
	// CSVの一マスを、ゲーム内でも1x1x1のブロックとして配置します。
	constexpr float kStageMapChipCellSize = 1.0f;
	// P0が従来と同じX=24、Z=35になるよう、CSV左上の座標を決めます。
	const Vector3 kStageMapChipOrigin{ 22.0f, 0.0f, 33.0f };
	// 高さ1.0のブロック上面をY=-2.0へ揃える中心高さです。
	constexpr float kStageMapChipFloorY = -2.5f;
	// Playerの球Colliderが床上面へ接する開始高さです。
	constexpr float kStageMapChipPlayerY = -0.8f;
	// 提出用Titleだけが使う、Edit Viewで決めた固定Cameraの位置と角度です。
	const Vector3 kTemporaryTitleCameraPosition{ 16.703f, 0.689f, 37.457f };
	const Vector3 kTemporaryTitleCameraRotationDegrees{ 0.335f, -183.0f, 0.0f };
}

// 仮Titleと本編の案内を別々に所有し、Sceneごとの表示条件を混ぜません。
Stage::Stage(int initialStageId, bool startsWithTemporaryTitle)
	: stageMapRepository_(initialStageId)
	, startsWithTemporaryTitle_(startsWithTemporaryTitle)
{
	if (startsWithTemporaryTitle_) {
		stageTitleOverlay_ = std::make_unique<StageTitleOverlay>();
	} else {
		stagePauseOverlay_ = std::make_unique<StagePauseOverlay>();
	}
}

// 共通Stageが所有するPlayer・Mirror・Rendererを、完全な型を知る場所で破棄します。
Stage::~Stage() = default;

// 3Dの必須部品と使用する画面の案内を順にそろえ、欠けたStageを描画へ進めません。
// 読込に失敗した時も選択中のStage番号をログに残し、別Stageの失敗をStage1と取り違えないようにします。
void Stage::Initialize()
{
	const std::string stageLogPrefix = "Stage " + std::to_string(GetStageId()) + " initialization failed: ";
	isStageReady_ = false;
	isTemporaryTitleActive_ = startsWithTemporaryTitle_;
	pauseState_ = PauseState::kPlaying;
	lightDeath_.Reset();
	isLeavingForGameOver_ = false;
	if (!InitializeRenderSystems()) {
		Logger::Log(stageLogPrefix + "light puzzle renderer could not be prepared.");
		PostQuitMessage(1);
		return;
	}
	// BGMの再生役だけを準備し、音声ファイルの読込はStage1画面の表示後まで遅らせます。
	stageBgm_.Initialize(Engine::Audio::GetInstance());
	stageLighting_.InitializeDefaults();
	if (!InitializeStageGimmicks()) {
		Logger::Log(stageLogPrefix + "required stage gimmick model, texture or light renderer could not be prepared.");
		PostQuitMessage(1);
		return;
	}
	// JSONが読めない時に予備座標の鏡を表示しないよう、Stage開始をここで止めます。
	if (!InitializeStageMap()) {
		Logger::Log(stageLogPrefix + "stage map could not be loaded.");
		PostQuitMessage(1);
		return;
	}
	if (!InitializePlayerAndCamera()) {
		Logger::Log(stageLogPrefix + "player or camera could not be prepared.");
		PostQuitMessage(1);
		return;
	}
	// 仮Titleと本編のどちらでも、完成したCameraを使う夜空を一度だけ生成します。
	skyBox_ = Engine::Object3dFactory::CreateSkyBox(
		Engine::DirectXCommon::GetInstance(), cameraManager_->GetActiveCamera(), kSkyBoxTexturePath);
	if (!skyBox_) {
		Logger::Log(stageLogPrefix + "SkyBox texture could not be loaded: " + kSkyBoxTexturePath);
		PostQuitMessage(1);
		return;
	}
	if (isTemporaryTitleActive_ &&
		!stageTitleOverlay_->Initialize(Engine::SpriteCommon::GetInstance(), Engine::DirectXCommon::GetInstance())) {
		// Titleの共通Pipelineが未完成なら描画へ進めず、素材欠損も同じ失敗として止めます。
		Logger::Log("Temporary title initialization failed: Sprite pipeline or texture could not be prepared.");
		PostQuitMessage(1);
		return;
	}
	if (stagePauseOverlay_ &&
		!stagePauseOverlay_->Initialize(Engine::SpriteCommon::GetInstance(), Engine::DirectXCommon::GetInstance())) {
		Logger::Log(stageLogPrefix + "pause Sprite pipeline or texture could not be prepared.");
		PostQuitMessage(1);
		return;
	}
	InitializeGameplaySmoke();
	isStageReady_ = true;
}

// Lightを先に準備し、描けない場合はCamera・Objectを作らず起動を止めます。
bool Stage::InitializeRenderSystems()
{
	Engine::DirectXCommon* dxCommon = Engine::DirectXCommon::GetInstance();
	if (!lightPuzzle_.Initialize(dxCommon)) {
		return false;
	}
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);

	// Playerが部屋を見る通常Cameraを、まず作成します。
	InitializeMainCamera(cameraManager_, mainCamera_, { 0.0f, 1.0f, -12.0f });
	object3dCommon_ = Engine::Object3dCommon::GetInstance();
	object3dCommon_->Initialize(dxCommon);
	object3dCommon_->SetDefaultCamera(cameraManager_->GetActiveCamera());
	sceneObjects_.Initialize(object3dCommon_);
	return true;
}

// Factoryへ従来の順番で生成を依頼し、描画に必要な部品がそろったか確認します。
bool Stage::InitializeStageGimmicks()
{
	const StageLightPuzzle::Settings& puzzleSettings = lightPuzzle_.GetSettings();
	Engine::DirectXCommon* dxCommon = Engine::DirectXCommon::GetInstance();

	// 床はJSON読込後に配置を上書きするため、ここでは読込失敗時用の初期値を置きます。
	floor_ = StageGimmickFactory::CreateFloor(sceneObjects_);
	if (!floor_) {
		return false;
	}

	// 実際の配置は後でstage1.jsonから反映します。Factoryは所持状態へ切り替えません。
	carryableMirror_ = StageMirrorFactory::CreateCarryableMirror(
		object3dCommon_,
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
		object3dCommon_,
		"debug/plane.obj",
		mirrorFloorPosition_,
		mirrorFloorWidth_,
		mirrorFloorHeight_);

	// 白い発射球二つの生成をまとめ、寿命はCollectionに残します。
	StageGimmickFactory::LaserEmitters emitters;
	if (!StageGimmickFactory::CreateLaserEmitters(sceneObjects_, puzzleSettings, emitters)) {
		return false;
	}
	laserEmitter_ = emitters.charge;
	doorLaserEmitter_ = emitters.door;

	// 危険Lightの色・太さ・描画用Rendererは、軌道と同じギミック部品が所有します。
	if (!hazardLights_.Initialize(dxCommon)) {
		return false;
	}

	// SwitchとDoorは危険LightのRenderer準備後に生成する順序を変えません。
	StageGimmickFactory::SwitchesAndDoor puzzleObjects;
	if (!StageGimmickFactory::CreateSwitchesAndDoor(sceneObjects_, puzzleSettings, puzzleObjects)) {
		return false;
	}
	chargeSwitch_ = puzzleObjects.chargeSwitch;
	doorSwitch_ = puzzleObjects.doorSwitch;
	lightDoor_ = puzzleObjects.lightDoor;
	return true;
}

// Stage1用JSONとCSVを読み、ColliderとHot Reload監視を開始します。
bool Stage::InitializeStageMap()
{
	if (!LoadStage(stageMapRepository_.GetStageId(), false)) {
		return false;
	}
	// 最初のPlayer更新より前に、閉じたDoorのColliderとLaser経路を作ります。
	UpdateLightPuzzle(0.0f);
	collisionWorld_.Rebuild({
		floor_,
		&fixedMirrors_,
		fixedMirrors_.empty() ? nullptr : carryableMirror_.get(),
		fixedMirrors_.empty() ? nullptr : &lightPuzzle_,
		&stageMapRuntime_,
		!fixedMirrors_.empty() && lightDoor_ != nullptr,
	});
	return true;
}

// CSV・JSONで開始位置が指定されなければ、従来の既定座標を全経路へ返します。
Vector3 Stage::GetStagePlayerStartPosition() const
{
	return hasStagePlayerStart_
		? stagePlayerStartPosition_
		: Vector3{ 0.0f, -0.8f, 5.0f };
}

// Player、通常追従Camera、開始演出を、この順番で作成します。
bool Stage::InitializePlayerAndCamera()
{
	const Vector3 playerStartPosition = GetStagePlayerStartPosition();
	player_ = std::make_unique<Player>();
	if (!player_->Initialize(object3dCommon_, "debug/sphere.obj", playerStartPosition, 1.2f)) {
		// Playerモデルを作れない状態で、Cameraや開始演出がnullptrを参照しないよう中断します。
		player_.reset();
		return false;
	}
	// sphere.obj本来のUVチェッカーを使わず、Playerだけへモンスターボール画像を明示します。
	if (!player_->GetObject().SetTextureOverride("resources/debug/monsterBall.png")) {
		player_.reset();
		return false;
	}

	// Camera本体とは別のControllerに、Playerを追従する規則を任せます。
	cameraController_ = std::make_unique<Engine::CameraController>();
	cameraController_->Initialize(mainCamera_.get(), player_->GetPosition());
	cameraController_->SetAutoRecenterEnabled(false);
	// 建物の外壁だけは越えないようにし、室内壁の向こうはシルエット表示を優先します。
	cameraController_->SetWallAvoidanceEnabled(true);

	// 本編Cameraの完成位置を保存し、Title中は登場演出をまだ進めません。
	stageStart_ = std::make_unique<StageStart>();
	stageStartGameplayCameraPosition_ = mainCamera_->GetTranslate();
	stageStartGameplayCameraRotate_ = mainCamera_->GetRotate();
	if (isTemporaryTitleActive_) {
		constexpr float kDegreeToRadian = std::numbers::pi_v<float> / 180.0f;
		mainCamera_->SetTranslate(kTemporaryTitleCameraPosition);
		mainCamera_->SetRotate({
			kTemporaryTitleCameraRotationDegrees.x * kDegreeToRadian,
			kTemporaryTitleCameraRotationDegrees.y * kDegreeToRadian,
			kTemporaryTitleCameraRotationDegrees.z * kDegreeToRadian,
		});
	} else {
		BeginStageStart();
	}
	stageEditor_.Initialize();
	return true;
}

// Title中は開始せず、Stage1本編の初期化時にPlayer登場演出を開始します。
void Stage::BeginStageStart()
{
	if (!stageStart_ || !player_ || !mainCamera_) {
		return;
	}
	const Vector3 playerStartPosition = GetStagePlayerStartPosition();
	stageStart_->Begin(
		*player_,
		*mainCamera_,
		playerStartPosition,
		stageStartGameplayCameraPosition_,
		stageStartGameplayCameraRotate_,
		stageStartSettings_);
}

// 設定値を変更せず自動動作確認へ渡し、テスト側で本編の反射経路を確認できるようにします。
void Stage::InitializeGameplaySmoke()
{
	const StageLightPuzzle::Settings& puzzleSettings = lightPuzzle_.GetSettings();
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
		object3dCommon_,
		[this](const std::vector<LaserSegment>& sourceSegments)
		{
			return ReflectHazardLightSegments(sourceSegments);
		},
	});
}

// 初期化途中でもTitle・Pause案内を含む生成済みの所有物だけを安全に解放します。
void Stage::Finalize()
{
	isStageReady_ = false;
	// unique_ptr がオブジェクトを自動的に解放します。
	// SceneManager がこの関数の前に GPU の処理完了を待機します。
	if (stageTitleOverlay_) {
		stageTitleOverlay_->Finalize();
	}
	if (stagePauseOverlay_) {
		stagePauseOverlay_->Finalize();
	}
	skyBox_.reset();
	stageCameraEvents_.Clear(cameraManager_.get());
	stageEditor_.Finalize();
	sceneObjects_.Clear();
	stageMapRuntime_.Clear();
	collisionWorld_.Clear();
	stageMapRepository_.Clear();
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

// 停止へ入ったフレームと解除したフレームも、従来どおり本編更新を次フレームまで待ちます。
bool Stage::UpdatePauseState()
{
	if (isTemporaryTitleActive_) {
		return false;
	}
	const bool isGameViewActive = ImGuiManager::GetInstance()->IsGameViewActive();
	Engine::Input* input = Engine::Input::GetInstance();
	if (pauseState_ == PauseState::kPlaying) {
		if (!isGameViewActive || !input->TriggerKey(DIK_TAB)) {
			return false;
		}
		pauseState_ = PauseState::kPaused;
	} else if (pauseState_ == PauseState::kPaused && isGameViewActive) {
		if (input->TriggerKey(DIK_BACK)) {
			if (Engine::SceneManager::GetInstance()->ChangeSceneWithFade("STAGE_SELECT")) {
				pauseState_ = PauseState::kLeavingToStageSelect;
			}
		} else if (input->TriggerAnyKeyExcept(DIK_BACK)) {
			pauseState_ = PauseState::kPlaying;
		}
	}
	// 停止中にもBegin/Endしたフレームだけ描画し、既存のImGui順を保ちます。
	ImGuiManager::GetInstance()->Begin("Stage1");
	ImGuiManager::GetInstance()->End();
	PrepareStageRenderObjects();
	return true;
}

// Pauseで早期終了するフレームも案内を更新し、PlayerやCameraの更新順は変えません。
void Stage::Update()
{
	if (!isStageReady_) {
		return;
	}
	if (stagePauseOverlay_) {
		stagePauseOverlay_->Update(
			static_cast<float>(Engine::DirectXCommon::GetInstance()->GetClientWidth()),
			static_cast<float>(Engine::DirectXCommon::GetInstance()->GetClientHeight()));
	}
	if (UpdatePauseState()) {
		return;
	}
	// 提出用TitleではStageの描画だけを準備し、Player・Camera追従・BGMは進めません。
	if (isTemporaryTitleActive_) {
		Engine::DirectXCommon* dxCommon = Engine::DirectXCommon::GetInstance();
		const float deltaTime = dxCommon->GetDeltaTime();
		stageTitleOverlay_->Update(
			deltaTime,
			static_cast<float>(dxCommon->GetClientWidth()),
			static_cast<float>(dxCommon->GetClientHeight()));
		// Pink/Yellowの危険Lightは本編と同じ軌道・反射・SpotLightを使い、Cameraだけ固定します。
		hazardLights_.Update(
			deltaTime,
			nullptr,
			[this](const std::vector<LaserSegment>& sourceSegments)
			{
				return ReflectHazardLightSegments(sourceSegments);
			});
		stageLighting_.UpdateLaserSpotLights(lightPuzzle_, hazardLights_);
		const bool startRequested =
			ImGuiManager::GetInstance()->IsGameViewActive() &&
			(Engine::Input::GetInstance()->TriggerKey(DIK_RETURN) ||
				Engine::Input::GetInstance()->IsPadButtonPressed(0, 3));
		if (startRequested) {
			Engine::SceneManager::GetInstance()->ChangeSceneWithFade("STAGE_SELECT");
		}
		cameraManager_->Update();
		UpdateReflectionCameras();
		// DevelopmentではBegin/EndしたFrameだけGame Viewへ描画結果を渡せます。
		ImGuiManager::GetInstance()->Begin("Stage1");
		ImGuiManager::GetInstance()->End();
		PrepareStageRenderObjects();
		return;
	}
	// ---------- Stage1マップのホットリロード ----------
	ApplyRequestedStageChange();
	UpdateStageMapHotReload();
	if (ImGuiManager::GetInstance()->IsGameViewActive()) {
		stageMapRuntime_.UpdatePaths(Engine::DirectXCommon::GetInstance()->GetDeltaTime());
	}

	// ---------- プレイヤーの移動と重力 ----------
	const float deltaTime = Engine::DirectXCommon::GetInstance()->GetDeltaTime();
	const bool wasLightDeathPlaying = lightDeath_.IsPlaying();
	if (wasLightDeathPlaying) {
		UpdateLightDeath(deltaTime);
	}
	const bool isStageStartPlaying = wasLightDeathPlaying ? false : UpdateStagePlayer(deltaTime);
	stageBgm_.Update(deltaTime);
	// 本編のDoor・危険Lightの結果だけを、自動確認部品へ渡します。
	if (!wasLightDeathPlaying) {
		gameplaySmoke_.Update(
			{
				&hazardLights_,
				&collisionWorld_.GetSolidObbs(),
				lightPuzzle_.IsDoorSwitchReceivingLight(),
				lightPuzzle_.GetDoorOpenAmount(),
			},
			deltaTime);
		if (!fixedMirrors_.empty()) {
			UpdateMirrorGameplay();
		}
	}
	if (!fixedMirrors_.empty()) {
		UpdateLightPuzzle(deltaTime, !wasLightDeathPlaying);
		// 危険Lightの軌道・反射・Player接触は、Stage固有ギミックへまとめて任せます。
		hazardLights_.Update(
			deltaTime,
			player_ && !wasLightDeathPlaying ? &player_->GetSphere() : nullptr,
			[this](const std::vector<LaserSegment>& sourceSegments)
			{
				return ReflectHazardLightSegments(sourceSegments);
			});
		// 見た目だけの線ではなく、反射後の経路も周囲を照らすSpotLightへ反映します。
		stageLighting_.UpdateLaserSpotLights(lightPuzzle_, hazardLights_);
		// 接触判定より先に警告の方向と範囲を更新し、死亡演出へそのまま引き継ぎます。
		if (player_ && !wasLightDeathPlaying) {
			stageLightWarning_.Update(*player_, lightPuzzle_, hazardLights_, deltaTime);
		}
		if (!wasLightDeathPlaying && !isStageStartPlaying &&
			(lightPuzzle_.IsPlayerHitByLaser() || hazardLights_.IsPlayerHit())) {
			if (player_) {
				lightDeath_.Begin(*player_, stageLightWarning_);
			}
		}
	} else {
		// 設計開始時のStageでは前StageのLaser光だけを残さず、共通の床と配置物を表示します。
		stageLighting_.UpdateLaserSpotLights(lightPuzzle_, hazardLights_);
	}

	// ---------- カメラとデバッグ UI の更新 ----------
	if (lightDeath_.IsPlaying()) {
		// 死亡位置からCameraを動かさず、既存の行列更新だけ行います。
		cameraManager_->Update();
	} else {
		UpdateStageCamera(deltaTime, isStageStartPlaying);
	}
	UpdateReflectionCameras();
	// 左のScene欄へ現在番号だけを渡し、切替要求はImGui描画後に予約します。
	ImGuiManager::GetInstance()->SetStageSelection(GetStageId());
	ImGuiManager::GetInstance()->Begin("Stage1");
	// Gameplay中もPlayerの球Colliderを表示し、Laser接触を黄色で確認できるようにする
	if (player_ && !lightDeath_.IsPlaying()) {
		ImGuiManager::GetInstance()->DrawPlayerCollisionDebug(
			player_->GetSphere(),
			cameraManager_->GetActiveCamera(),
			player_->IsColliding(),
			lightPuzzle_.IsPlayerHitByLaser() || hazardLights_.IsPlayerHit());
	}
	DrawStagePuzzleDebugUi();
	DrawStageEditorUi();
	ImGuiManager::GetInstance()->End();
	const int nextStageId = ImGuiManager::GetInstance()->TakeRequestedStageId();
	if (nextStageId >= 1 && nextStageId != GetStageId()) {
		// 切替前の編集中データを現在StageのJSONへ保存し、失敗したら番号を変えません。
		if (SaveStageMap()) {
			RequestStage(nextStageId);
		}
	}

	PrepareStageRenderObjects();
}

// Sceneは所有対象を渡す順番だけを決め、共通の描画準備はStageRenderContextへ任せます。
void Stage::PrepareStageRenderObjects()
{
	StageRenderContext::Context renderContext{
		cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr,
		stageLighting_,
		sceneObjects_.GetObjects(),
		stageMapRuntime_,
		fixedMirrors_,
		mirrorFloor_.get(),
		carryableMirror_.get(),
		player_.get(),
		*skyBox_ };
	StageRenderContext::Prepare(renderContext);
}

// 3Dの後に本編のTab案内とPause案内を重ね、反射と画面提示の順番を保ちます。
void Stage::Draw()
{
	if (!isStageReady_) {
		return;
	}
	// ---------- 固定鏡の反射Textureを先に作成 ----------
	Engine::SrvManager::GetInstance()->PreDraw();
	StageReflectionRenderer::Context reflectionContext{};
	reflectionContext.directXCommon = Engine::DirectXCommon::GetInstance();
	reflectionContext.object3dCommon = object3dCommon_;
	reflectionContext.activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
	reflectionContext.sceneObjects = &sceneObjects_.GetObjects();
	reflectionContext.stageMapRuntime = &stageMapRuntime_;
	reflectionContext.fixedMirrors = &fixedMirrors_;
	reflectionContext.mirrorFloor = fixedMirrors_.empty() ? nullptr : mirrorFloor_.get();
	reflectionContext.carryableMirror = fixedMirrors_.empty() ? nullptr : carryableMirror_.get();
	reflectionContext.player = player_.get();
	reflectionContext.lightPuzzle = &lightPuzzle_;
	reflectionContext.hazardLights = &hazardLights_;
	StageReflectionRenderer::DrawOne(reflectionContext, reflectionUpdateCursor_);

	// ---------- ゲーム画面への描画 ----------
	Engine::DirectXCommon::GetInstance()->PreDraw();
	StageSceneRenderer::Context sceneRendererContext{};
	sceneRendererContext.object3dCommon = object3dCommon_;
	sceneRendererContext.activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
	sceneRendererContext.skyBox = skyBox_.get();
	sceneRendererContext.sceneObjects = &sceneObjects_.GetObjects();
	sceneRendererContext.floor = floor_;
	sceneRendererContext.drawStagePuzzle = !fixedMirrors_.empty();
	sceneRendererContext.stageMapRuntime = &stageMapRuntime_;
	sceneRendererContext.fixedMirrors = &fixedMirrors_;
	sceneRendererContext.mirrorFloor = fixedMirrors_.empty() ? nullptr : mirrorFloor_.get();
	sceneRendererContext.carryableMirror = fixedMirrors_.empty() ? nullptr : carryableMirror_.get();
	sceneRendererContext.player = player_.get();
	sceneRendererContext.lightPuzzle = &lightPuzzle_;
	sceneRendererContext.hazardLights = &hazardLights_;
	StageSceneRenderer::Draw(sceneRendererContext);
	if (isTemporaryTitleActive_) {
		// 3Dの後に透過Spriteを重ね、指定したStage1の景色を隠さずにTitleを見せます。
		SceneRenderPipeline::DrawSprites(
			Engine::SpriteCommon::GetInstance(), stageTitleOverlay_->GetSprites());
	}
	if (stagePauseOverlay_) {
		// Tabは本編とPauseの両方で表示し、停止時だけ二枚の案内を追加します。
		SceneRenderPipeline::DrawSprites(
			Engine::SpriteCommon::GetInstance(), stagePauseOverlay_->GetTabSprites());
		if (pauseState_ != PauseState::kPlaying) {
			SceneRenderPipeline::DrawSprites(
				Engine::SpriteCommon::GetInstance(), stagePauseOverlay_->GetPauseSprites());
		}
	}

	// PostEffect・SwapChain・ImGui・Presentは、全Scene共通のPipelineへ任せます。
	SceneRenderPipeline::End(
		Engine::DirectXCommon::GetInstance(),
		cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr);
	// 次のUpdateからBGMを読み込めるよう、最初の本編画面が出たことを記録します。
	stageBgm_.MarkFrameDrawn();
}

// 開始演出・自動確認・通常入力のいずれかでPlayerを更新し、開始演出中ならtrueを返します。
bool Stage::UpdateStagePlayer(float deltaTime)
{
	if (!player_ || !floor_) {
		return false;
	}

	collisionWorld_.Rebuild({
		floor_,
		&fixedMirrors_,
		fixedMirrors_.empty() ? nullptr : carryableMirror_.get(),
		fixedMirrors_.empty() ? nullptr : &lightPuzzle_,
		&stageMapRuntime_,
		!fixedMirrors_.empty() && lightDoor_ != nullptr,
	});
	const bool isGameViewActive = ImGuiManager::GetInstance()->IsGameViewActive();
	const bool isStageStartPlaying =
		isGameViewActive &&
		stageStart_ &&
		stageStart_->Update(deltaTime, *player_, *mainCamera_);
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
		!fixedMirrors_.empty() && carryableMirror_ &&
		carryableMirror_->IsCarried() &&
		(Engine::Input::GetInstance()->IsMouseButtonPressed(0) ||
			Engine::Input::GetInstance()->IsMouseButtonPressed(1));
	player_->SetMirrorGuardMode(isMirrorGuarding);
	// 現在画面に映しているCameraの正面を渡し、WASDを画面基準の移動へ変換します。
	Vector3 cameraForward{ 0.0f, 0.0f, 1.0f };
	if (cameraManager_) {
		if (Engine::Camera* activeCamera = cameraManager_->GetActiveCamera()) {
			cameraForward = GetCameraForward(*activeCamera);
		}
	}
	player_->Update(deltaTime, collisionWorld_.GetSolidObbs(), cameraForward);
	return false;
}

// 部品が一秒の消失を終えた後だけ、Stageが復帰位置と次Sceneを決めます。
void Stage::UpdateLightDeath(float deltaTime)
{
	if (!player_ || isLeavingForGameOver_) {
		return;
	}
	if (!lightDeath_.Update(*player_, stageLightWarning_, deltaTime)) {
		return;
	}

	const Vector3 respawnPosition = GetStagePlayerStartPosition();
	// ワープ直前のCamera位置を保存し、復帰後の同じ向き・距離をPlayerの初期位置へ移します。
	Engine::Camera* deathCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
	const Vector3 deathCameraPosition = deathCamera ? deathCamera->GetTranslate() : Vector3{};
	const Vector3 deathCameraRotation = deathCamera ? deathCamera->GetRotate() : Vector3{};
	player_->ResetForRespawn(respawnPosition);
	if (lightDeath_.GetDeathCount() >= 3) {
		// 切替フェード中にも初期位置のPlayerが一瞬映らないよう、透明のまま待機します。
		player_->GetObject().SetOpacity(0.0f);
		stageLightWarning_.UpdateDeath(*player_, 1.0f);
		GameOverScene::SetRetryStageId(stageMapRepository_.GetStageId());
		isLeavingForGameOver_ = Engine::SceneManager::GetInstance()->ChangeSceneWithFade("GAME_OVER");
		return;
	}
	stageLightWarning_.Reset(*player_);
	if (cameraManager_ && mainCamera_) {
		stageCameraEvents_.RestoreMainCamera(*cameraManager_);
		mainCamera_->SetTranslate({
			deathCameraPosition.x + respawnPosition.x - lightDeath_.GetStartPosition().x,
			deathCameraPosition.y + respawnPosition.y - lightDeath_.GetStartPosition().y,
			deathCameraPosition.z + respawnPosition.z - lightDeath_.GetStartPosition().z,
		});
		mainCamera_->SetRotate(deathCameraRotation);
		if (cameraController_) {
			cameraController_->SynchronizeToCamera(respawnPosition);
		}
	}
	lightDeath_.CompleteRespawn();
}

// 固定鏡の位置・向きから、各鏡面用Cameraの反射行列を更新します。
void Stage::UpdateReflectionCameras()
{
	Engine::Camera* sourceCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
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
void Stage::UpdateLightPuzzle(float deltaTime, bool checkPlayerCollision)
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
		player_ && checkPlayerCollision ? &player_->GetSphere() : nullptr,
		fixedMirrors_.front().get(),
		laserEmitter_,
		doorLaserEmitter_,
		chargeSwitch_,
		doorSwitch_,
		lightDoor_);
}

// 開始演出・通常追従・Event Cameraの優先順位で、Stage1のCameraを更新します。
void Stage::UpdateStageCamera(float deltaTime, bool isStageStartPlaying)
{
	if (!cameraManager_) {
		return;
	}

	if (ImGuiManager::GetInstance()->IsGameViewActive() && !isStageStartPlaying) {
		UpdateMainCamera();
		if (stageStart_ && mainCamera_ && stageStart_->IsCameraHandoffPlaying()) {
			// 通常Cameraの壁回避・追従結果へ、開始演出Cameraを滑らかに近づけます。
			stageStart_->UpdateCameraHandoff(deltaTime, *mainCamera_);
		}
		if (player_) {
			stageCameraEvents_.UpdateEventCamera(
				*player_,
				*cameraManager_,
				collisionWorld_.GetCameraBoundaryObbs(),
				deltaTime);
			if (const std::string status = stageCameraEvents_.ConsumeStatus(); !status.empty()) {
				stageEditor_.SetReloadStatus(status);
			}
		}
	} else if (!stageCameraEvents_.GetActiveEventCameraName().empty()) {
		// Edit Viewではイベントカメラへ自動切替せず、編集用のMainCameraを維持します。
		stageCameraEvents_.RestoreMainCamera(*cameraManager_);
	}
	cameraManager_->Update();
}

// Player位置・向き・壁回避の結果から、通常追従Cameraを更新します。
void Stage::UpdateMainCamera()
{
	if (!player_ || !cameraController_) {
		return;
	}
	stageCameraEvents_.UpdateCameraArea(*player_, *cameraController_);
	// 携帯Mirrorを持っている間は、構え操作でCameraの通常距離が近付かないようにします。
	// 壁がCameraとPlayerの間にある時だけは、CameraControllerの壁回避が優先されます。
	cameraController_->SetDistanceLock(carryableMirror_ && carryableMirror_->IsCarried());
	if (const std::string status = stageCameraEvents_.ConsumeStatus(); !status.empty()) {
		stageEditor_.SetReloadStatus(status);
	}

	// 通常時のCameraは、矢印キー一回につき一段階だけ位置を変更する
	Engine::Input* input = Engine::Input::GetInstance();
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
		Engine::DirectXCommon::GetInstance()->GetDeltaTime(),
		player_->GetPosition(),
		player_->GetMoveDirection(),
		isCameraStepInput,
		collisionWorld_.GetCameraBoundaryObbs());
}

// Cameraのワールド行列から、Player移動に使う正面方向を返します。
Vector3 Stage::GetCameraForward(const Engine::Camera& camera) const
{
	// このプロジェクトでは、カメラのローカル座標 +Z が正面です。
	const Matrix4x4& worldMatrix = camera.GetWorldMatrix();
	return Normalize({ worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2] });
}

// Player開始位置、持てるMirror位置、StageStart演出設定を読込済みLevelDataから反映します。
void Stage::ApplyStageStartSettings(
	const LevelLoader::ObjectData* playerStartData,
	const LevelLoader::ObjectData* carryableMirrorData,
	const MapChipField::Chip* csvPlayerStartChip)
{
	// P0がCSVにあればJSONのPlayerStartより優先します。
	// Hot Reload時にはPlayerを移動させないため、実際の配置はInitializeで一度だけ行います。
	hasStagePlayerStart_ = csvPlayerStartChip != nullptr || playerStartData != nullptr;
	if (csvPlayerStartChip) {
		stagePlayerStartPosition_ = stageMapRepository_.GetMapChipField().GetPosition(
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
	// Stage間で未指定の開始演出値を引き継がないよう、各読込の既定値へ戻します。
	stageStartSettings_ = {};
	const LevelLoader::LevelData* levelData = stageMapRepository_.GetLevelData();
	if (levelData && levelData->hasStageStart) {
		stageStartSettings_.duration = levelData->stageStart.duration;
		stageStartSettings_.playerAirHeight = levelData->stageStart.playerAirHeight;
		stageStartSettings_.cameraFrontDistance = levelData->stageStart.cameraFrontDistance;
		stageStartSettings_.cameraFrontHeight = levelData->stageStart.cameraFrontHeight;
		stageStartSettings_.cameraOrbitAngle = levelData->stageStart.cameraOrbitAngle;
		stageStartSettings_.cameraHandoffDuration = levelData->stageStart.cameraHandoffDuration;
	}
}

// Editorで変更した通常3D配置物とCamera Eventを、再生成せず反映します。
void Stage::ApplyStageRuntimeEdits(
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
void Stage::ApplyFloorData(const LevelLoader::ObjectData& floorData)
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
bool Stage::ApplyStageMapData(bool rebuildRuntimeObjects)
{
	// JSONのObjectDataを分類し、既存の床・鏡・Camera・追加モデルへ反映する中心処理です。
	LevelLoader::LevelData* levelData = stageMapRepository_.GetLevelData();
	if (!levelData || !floor_) {
		return false;
	}

	// JSONのタグとobject_typeの分類は、SceneではなくLevel用のIndexへ任せます。
	StageMapObjectIndex objectIndex;
	objectIndex.Build(*levelData);
	const LevelLoader::ObjectData* floorData = objectIndex.GetFloor();
	const LevelLoader::ObjectData* playerStartData = objectIndex.GetPlayerStart();
	const LevelLoader::ObjectData* carryableMirrorData = objectIndex.GetCarryableMirror();
	const std::vector<const LevelLoader::ObjectData*>& mirrorDataList = objectIndex.GetFixedMirrors();
	std::vector<const LevelLoader::ObjectData*> additionalObjects = objectIndex.GetRuntimeObjects();
	const std::vector<const LevelLoader::ObjectData*>& eventTriggerDataList = objectIndex.GetEventTriggers();
	const std::vector<const LevelLoader::ObjectData*>& eventCameraDataList = objectIndex.GetEventCameras();
	const std::vector<const LevelLoader::ObjectData*>& cameraAreaDataList = objectIndex.GetCameraAreas();
	// Chipの対応表とObjectData生成はFactoryへ任せ、Sceneは採用順だけを決めます。
	StageMapChipFactory::StageChipData mapChipData = StageMapChipFactory::BuildStageData(
		stageMapRepository_.GetMapChipField(), kStageMapChipOrigin, kStageMapChipCellSize, kStageMapChipFloorY);
	for (const LevelLoader::ObjectData& blockData : mapChipData.blockData) {
		additionalObjects.push_back(&blockData);
	}

	// 床はPlayerの足場として必須です。固定鏡は設計を始めたばかりのStageでは空も許します。
	if (!floorData) {
		return false;
	}

	// Mirrorタグの数だけ固定鏡を作るため、JSONへMirrorを追加すれば複数配置できます。
	const bool rebuildFixedMirrors =
		rebuildRuntimeObjects || fixedMirrors_.size() != mirrorDataList.size();
	std::vector<std::unique_ptr<FixedMirror>> rebuiltFixedMirrors;
	if (rebuildFixedMirrors) {
		if (!StageMirrorFactory::CreateFixedMirrors(object3dCommon_, mirrorDataList, rebuiltFixedMirrors)) {
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
	// Runtime自身がFactoryで仮生成し、成功した一覧だけを後で採用します。
	if (rebuildRuntimeObjects && !rebuiltMapRuntime.Rebuild(additionalObjects, object3dCommon_)) {
		return false;
	}

	// 失敗し得る生成が終わってから、Sceneが所有する状態をまとめて更新します。
	ApplyStageStartSettings(playerStartData, carryableMirrorData, mapChipData.playerStartChip);
	// Lighting未指定のStageへ、直前Stageの色や明るさを持ち越しません。
	stageLighting_.InitializeDefaults();
	stageLighting_.ApplyLevel(levelData->hasLighting ? &levelData->lighting : nullptr);
	ApplyFloorData(*floorData);

	// ---------- 鏡データの反映 ----------
	if (rebuildFixedMirrors) {
		fixedMirrors_ = std::move(rebuiltFixedMirrors);
	} else if (!StageMirrorFactory::ApplyFixedMirrorEdits(mirrorDataList, fixedMirrors_)) {
		return false;
	}
	// Inspector・JSONで変えた基準角度をPuzzleにも渡し、次の更新で元へ戻るのを防ぎます。
	// プレイ中の充電による回転量は保持し、Levelデータとは分けて扱います。
	if (!mirrorDataList.empty()) {
		StageLightPuzzle::Settings puzzleSettings = lightPuzzle_.GetSettings();
		puzzleSettings.largeMirrorBaseYaw = mirrorDataList.front()->rotation.y;
		lightPuzzle_.SetSettings(puzzleSettings);
		fixedMirrors_.front()->SetOrientation(
			fixedMirrors_.front()->GetPitch(),
			puzzleSettings.largeMirrorBaseYaw +
			puzzleSettings.largeMirrorTargetYawOffset * lightPuzzle_.GetLargeMirrorRotationAmount());
	}

	if (rebuildRuntimeObjects) {
		stageMapRuntime_ = std::move(rebuiltMapRuntime);
		stageCameraEvents_.Rebuild(
			eventTriggerDataList,
			eventCameraDataList,
			cameraAreaDataList,
			cameraManager_.get(),
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
