#include "Stage1GameplaySmoke.h"

#include "CameraController.h"
#include "CameraManager.h"
#include "CarryableMirror.h"
#include "Collider.h"
#include "Collision.h"
#include "EnemyManager.h"
#include "FixedMirror.h"
#include "Laser.h"
#include "Object3d.h"
#include "Object3dFactory.h"
#include "Player.h"
#include "StageCollisionWorld.h"
#include "StageHazardLights.h"
#include "StageLightDeath.h"
#include "StageLightWarning.h"
#include "StageMapRepository.h"
#include "StageMapRuntime.h"
#include "StageMirrorFactory.h"
#include "editor/StageLevelEditor.h"
#include "editor/StageEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <fstream>
#include <utility>
#include <windows.h>

// 数学関数と判定形状は、この実装ファイル内だけで短い名前を使います。
// ヘッダへusing namespaceを置かないため、ほかのクラスへ名前空間の影響は漏れません。
using namespace MyMath;

namespace
{
	// 環境変数を"1"にした時だけ、自動確認を実行します。
	bool IsEnvironmentEnabled(const char* name)
	{
		char* value = nullptr;
		size_t size = 0;
		if (_dupenv_s(&value, &size, name) != 0 || !value) {
			return false;
		}
		const bool enabled = std::string(value) == "1";
		std::free(value);
		return enabled;
	}
	// 開始演出と床外への移動時間を含めた、自動確認の上限時間です。
	constexpr float kGameplaySmokeTimeoutSeconds = 12.0f;
	// 高フレームレート時に秒数より先に終了しないための、安全用フレーム上限です。
	constexpr int kGameplaySmokeMaximumFrameCount = 10000;

	// 本編を動かさず、横・下・回転Lightの周期と切替の境目を確認します。
	bool CheckHazardLightTiming()
	{
		StageHazardLights lights;
		const auto advance = [&lights](float seconds) {
			lights.Update(seconds, nullptr, {});
			const auto& segments = lights.GetHorizontalMoveSegments();
			return segments.empty() ? -1.0f : segments.front().start.z;
		};
		const auto nearPosition = [](float actual, float expected) {
			return std::abs(actual - expected) < 0.001f;
		};
		// 回転中の角度ではなく中心からの距離を調べ、3本の広がりを確認します。
		const auto orbitHasRadius = [&lights, &nearPosition](float radius) {
			const auto& segments = lights.GetOrbitSegments();
			return segments.size() == 3 && std::all_of(segments.begin(), segments.end(), [&](const LaserSegment& segment) {
				const float x = segment.start.x - 24.0f;
				const float z = segment.start.z - 62.0f;
				return nearPosition(std::sqrt(x * x + z * z), radius);
			});
		};
		const bool startsNear = nearPosition(advance(0.0f), 30.0f) && !lights.GetBottomPulseSegments().empty();
		const bool orbitStartsClosed = orbitHasRadius(1.2f);
		advance(2.0f);
		const bool orbitFullyOpen = orbitHasRadius(5.0f);
		const bool waitsNear = nearPosition(advance(1.0f), 30.0f);
		const bool movesForward = nearPosition(advance(1.0f), 42.5f);
		const bool orbitClosesAfterCycle = orbitHasRadius(1.2f);
		const bool reachesFar = nearPosition(advance(1.0f), 55.0f) && lights.GetBottomPulseSegments().empty();
		const bool waitsFar = nearPosition(advance(3.0f), 55.0f);
		const bool movesBack = nearPosition(advance(1.0f), 42.5f);
		const bool returnsNear = nearPosition(advance(1.0f), 30.0f);
		advance(2.0f);
		const bool warns = !lights.GetBottomPulseWarningSegments().empty() && lights.GetBottomPulseSegments().empty();
		advance(3.0f);
		const bool restarts = !lights.GetBottomPulseSegments().empty() && lights.GetBottomPulseWarningSegments().empty();
		return startsNear && orbitStartsClosed && orbitFullyOpen && orbitClosesAfterCycle &&
			waitsNear && movesForward && reachesFar && waitsFar && movesBack && returnsNear && warns && restarts;
	}

	// 本編Playerを動かさず、接触一回・一秒の上昇・復帰後の再開を検証します。
	bool CheckStageLightDeath()
	{
		Player player;
		player.SetPosition({ 2.0f, 3.0f, 4.0f });
		player.SetMirrorGuardMode(true);
		StageLightWarning warning;
		StageLightDeath death;
		death.Begin(player, warning);
		death.Begin(player, warning);
		const bool countedOnce = death.IsPlaying() && death.GetDeathCount() == 1 &&
			!player.IsMirrorGuardMode() &&
			death.GetStartPosition().x == 2.0f &&
			death.GetStartPosition().y == 3.0f;
		const bool rising = !death.Update(player, warning, 0.55f) &&
			std::abs(player.GetPosition().y - 7.0f) < 0.001f;
		const bool completed = death.Update(player, warning, 0.45f) &&
			std::abs(player.GetPosition().y - 7.0f) < 0.001f;
		// Scene切替を予約できないフレームでも、次のUpdateで完了を再確認できます。
		const bool remainsCompleted = death.Update(player, warning, 0.1f) &&
			death.IsPlaying() && death.GetDeathCount() == 1;
		death.CompleteRespawn();
		const bool resumed = !death.IsPlaying() && death.GetDeathCount() == 1;
		death.Begin(player, warning);
		const bool nextContact = death.IsPlaying() && death.GetDeathCount() == 2;
		death.Reset();
		return countedOnce && rising && completed && remainsCompleted && resumed && nextContact &&
			!death.IsPlaying() && death.GetDeathCount() == 0;
	}

	// ColliderのフラグをOBB一覧へ変換する時に失わないか、実素材を使わず確認します。
	bool CheckStageMapColliderFlags()
	{
		Engine::Object3d floor;
		floor.SetScale({ 1.0f, 1.0f, 1.0f });
		StageMapRuntime mapRuntime;
		StageMapRuntime::RuntimeObject runtimeObject{};
		runtimeObject.tag = "CameraBoundary";
		runtimeObject.visual = std::make_unique<Engine::Object3d>();
		runtimeObject.visual->SetScale({ 1.0f, 1.0f, 1.0f });
		runtimeObject.hasBoxCollider = true;
		runtimeObject.collider.SetLocalShape({}, { 1.0f, 1.0f, 1.0f });
		mapRuntime.GetObjects().push_back(std::move(runtimeObject));
		ObbCollider& collider = mapRuntime.GetObjects().front().collider;
		StageCollisionWorld collisionWorld;
		StageCollisionWorld::Context context{};
		context.floor = &floor;
		context.stageMapRuntime = &mapRuntime;

		collisionWorld.Rebuild(context);
		const bool enabledIsSolid = collisionWorld.GetSolidObbs().size() == 2 &&
			collisionWorld.GetCameraBoundaryObbs().size() == 1 &&
			collisionWorld.GetLightBlockingObbs().size() == 2;
		collider.SetEnabled(false);
		collisionWorld.Rebuild(context);
		const bool disabledIsIgnored = collisionWorld.GetSolidObbs().size() == 1 &&
			collisionWorld.GetCameraBoundaryObbs().empty() &&
			collisionWorld.GetLightBlockingObbs().size() == 1;
		collider.SetEnabled(true);
		collider.SetTrigger(true);
		collisionWorld.Rebuild(context);
		const bool triggerIsIgnored = collisionWorld.GetSolidObbs().size() == 1 &&
			collisionWorld.GetCameraBoundaryObbs().empty() &&
			collisionWorld.GetLightBlockingObbs().size() == 1;
		return enabledIsSolid && disabledIsIgnored && triggerIsIgnored;
	}

	// 二枚目の編集データが不正でも、一枚目の既存Mirrorを変更しないことを確認します。
	bool CheckMirrorEditsRejectPartialUpdate()
	{
		std::vector<std::unique_ptr<FixedMirror>> mirrors;
		mirrors.push_back(std::make_unique<FixedMirror>());
		mirrors.push_back(std::make_unique<FixedMirror>());
		mirrors.front()->SetSurface({ 1.0f, 2.0f, 3.0f }, 0.0f, 4.0f, 4.0f);
		LevelLoader::ObjectData firstEdit{};
		firstEdit.translation = { 4.0f, 5.0f, 6.0f };
		const std::vector<const LevelLoader::ObjectData*> edits{ &firstEdit, nullptr };
		const bool rejected = !StageMirrorFactory::ApplyFixedMirrorEdits(edits, mirrors);
		const Vector3& center = mirrors.front()->GetMirror().GetCenter();
		return rejected && center.x == 1.0f && center.y == 2.0f && center.z == 3.0f;
	}

	// Edit Viewで一枚目の鏡を動かした時、入れ子のJSONだけを同期して二枚目を保ちます。
	bool CheckStageEditorMirrorSynchronization()
	{
		LevelLoader::LevelData levelData{};
		LevelLoader::ObjectData parent{};
		parent.tag = "Group";
		LevelLoader::ObjectData firstMirror{};
		firstMirror.tag = "Mirror";
		parent.children.push_back(std::move(firstMirror));
		levelData.objects.push_back(std::move(parent));
		LevelLoader::ObjectData secondMirror{};
		secondMirror.tag = "Mirror";
		secondMirror.translation = { 9.0f, 9.0f, 9.0f };
		levelData.objects.push_back(std::move(secondMirror));

		FixedMirror fixedMirror;
		fixedMirror.SetSurface({ 1.0f, 2.0f, 3.0f }, 0.0f, 4.0f, 6.0f);
		StageEditor editor;
		const bool synchronized = editor.SynchronizeFirstMirror(levelData, fixedMirror, 0.25f);
		const LevelLoader::ObjectData& edited = levelData.objects.front().children.front();
		const LevelLoader::ObjectData& unchanged = levelData.objects.back();
		LevelLoader::LevelData noMirrorData{};
		return synchronized &&
			edited.translation.x == 1.0f && edited.translation.y == 2.0f && edited.translation.z == 3.0f &&
			edited.rotation.y == 0.25f && edited.scaling.x == 2.0f && edited.scaling.y == 3.0f &&
			unchanged.translation.x == 9.0f && unchanged.translation.y == 9.0f &&
			!editor.SynchronizeFirstMirror(noMirrorData, fixedMirror, 0.25f);
	}

	// Editorの再構築失敗時に、削除・一括削除・追加が元のJSONと選択を保つか確認します。
	bool CheckStageEditorFailureRollback()
	{
		LevelLoader::LevelData levelData{};
		LevelLoader::ObjectData firstObject{};
		firstObject.name = "Existing";
		firstObject.tag = "MapObject";
		LevelLoader::ObjectData addedObject{};
		addedObject.name = "EditorAdded1";
		addedObject.tag = "EditorAdded";
		levelData.objects.push_back(std::move(firstObject));
		levelData.objects.push_back(std::move(addedObject));
		int selectedIndex = 1;
		int applyCalls = 0;
		int saveCalls = 0;
		std::string status;
		StageLevelEditor::Context context{};
		context.levelData = &levelData;
		context.selectedObjectIndex = &selectedIndex;
		context.applyLevelData = [&applyCalls](bool)
		{
			++applyCalls;
			return false;
		};
		context.saveLevelData = [&saveCalls]()
		{
			++saveCalls;
			return true;
		};
		context.setStatus = [&status](const std::string& message)
		{
			status = message;
		};
		const auto isOriginalState = [&]()
		{
			return levelData.objects.size() == 2 &&
				levelData.objects[0].name == "Existing" &&
				levelData.objects[1].name == "EditorAdded1" &&
				selectedIndex == 1;
		};
		StageLevelEditor editor;
		const bool removeRejected = !editor.RemoveSelectedObject(context) && isOriginalState();
		const bool clearRejected = editor.ClearEditorAddedObjects(context) ==
			"Clear failed. Current stage was kept." && isOriginalState() && saveCalls == 0;
		const bool addRejected = !editor.AddCameraArea(context) && isOriginalState();
		const bool pairRejected = !editor.AddEventPair(context) && isOriginalState();
		// 再構築は成功して保存だけ失敗した時、保存済みと誤表示しないことも確認します。
		context.applyLevelData = [](bool) { return true; };
		context.saveLevelData = [&saveCalls]()
		{
			++saveCalls;
			return false;
		};
		const std::string clearMessage = editor.ClearEditorAddedObjects(context);
		const bool saveFailureReported = levelData.objects.size() == 1 &&
			levelData.objects.front().name == "Existing" && selectedIndex == 0 &&
			saveCalls == 1 && status == "Models cleared in memory, but Save Map failed." &&
			clearMessage == status;
		return removeRejected && clearRejected && addRejected && pairRejected &&
			saveFailureReported && applyCalls == 4;
	}

	// 実モデルに合うBOX判定と改名がRuntimeへ届き、不正対象と反映失敗では元のデータを保つか確認します。
	bool CheckStageEditorModelMetadata(
		Engine::Object3dCommon* object3dCommon,
		bool& playerCollision)
	{
		playerCollision = false;
		LevelLoader::LevelData levelData{};
		LevelLoader::ObjectData group{};
		group.name = "Group";
		LevelLoader::ObjectData child{};
		child.name = "UsedInside";
		group.children.push_back(std::move(child));
		LevelLoader::ObjectData nestedEditorName{};
		nestedEditorName.name = "EditorModel2";
		group.children.push_back(std::move(nestedEditorName));
		levelData.objects.push_back(std::move(group));
		LevelLoader::ObjectData added{};
		added.type = "MESH";
		added.name = "EditorModel1";
		added.tag = "EditorAdded";
		added.objectType = "STATIC";
		added.fileName = "debug/floor.obj";
		levelData.objects.push_back(std::move(added));
		int selectedIndex = 1;
		StageMapRuntime runtime;
		StageMapRuntime::RuntimeObject runtimeObject{};
		runtimeObject.sourceName = "EditorModel1";
		runtimeObject.visual = Engine::Object3dFactory::Create(object3dCommon, levelData.objects[1].fileName);
		if (!runtimeObject.visual || !runtimeObject.visual->GetModel()) {
			return false;
		}
		const auto& modelVertices = runtimeObject.visual->GetModel()->GetModelData().vertices;
		if (modelVertices.empty()) {
			return false;
		}
		Vector3 expectedMin{
			modelVertices.front().position.x,
			modelVertices.front().position.y,
			modelVertices.front().position.z,
		};
		Vector3 expectedMax = expectedMin;
		for (const auto& vertex : modelVertices) {
			expectedMin.x = (std::min)(expectedMin.x, vertex.position.x);
			expectedMin.y = (std::min)(expectedMin.y, vertex.position.y);
			expectedMin.z = (std::min)(expectedMin.z, vertex.position.z);
			expectedMax.x = (std::max)(expectedMax.x, vertex.position.x);
			expectedMax.y = (std::max)(expectedMax.y, vertex.position.y);
			expectedMax.z = (std::max)(expectedMax.z, vertex.position.z);
		}
		runtime.GetObjects().push_back(std::move(runtimeObject));
		StageLevelEditor::Context context{};
		context.levelData = &levelData;
		context.selectedObjectIndex = &selectedIndex;
		context.mapRuntime = &runtime;
		context.applyLevelData = [&](bool rebuild)
		{
			if (rebuild) {
				runtime.GetObjects().front().sourceName = levelData.objects[1].name;
			}
			runtime.ApplyEdits({ &levelData.objects[1] });
			return true;
		};
		StageLevelEditor editor;
		const bool rejectedDuplicate = !editor.RenameSelectedObject(context, "UsedInside") &&
			levelData.objects[1].name == "EditorModel1";
		const bool rejectedBlank = !editor.RenameSelectedObject(context, "   ") &&
			levelData.objects[1].name == "EditorModel1";
		const bool renamed = editor.RenameSelectedObject(context, "HallFloor") &&
			levelData.objects[1].name == "HallFloor" &&
			runtime.GetObjects().front().sourceName == "HallFloor";
		// Xだけの一致では縦方向の薄い床を検証できないため、三軸の中心と寸法を確認します。
		const auto matchesModelBounds = [&]()
		{
			const LevelLoader::ColliderData& collider = levelData.objects[1].collider;
			const auto nearlyEqual = [](float first, float second)
			{
				return std::abs(first - second) < 0.001f;
			};
			return nearlyEqual(collider.center.x, (expectedMin.x + expectedMax.x) * 0.5f) &&
				nearlyEqual(collider.center.y, (expectedMin.y + expectedMax.y) * 0.5f) &&
				nearlyEqual(collider.center.z, (expectedMin.z + expectedMax.z) * 0.5f) &&
				nearlyEqual(collider.size.x, (std::max)(expectedMax.x - expectedMin.x, 0.1f)) &&
				nearlyEqual(collider.size.y, (std::max)(expectedMax.y - expectedMin.y, 0.1f)) &&
				nearlyEqual(collider.size.z, (std::max)(expectedMax.z - expectedMin.z, 0.1f));
		};
		const bool addedCollider = editor.AddBoxCollider(context) &&
			levelData.objects[1].hasCollider && levelData.objects[1].collider.type == "BOX" &&
			runtime.GetObjects().front().hasBoxCollider && matchesModelBounds();
		Engine::Object3d floor;
		StageCollisionWorld collisionWorld;
		StageCollisionWorld::Context collisionContext{};
		collisionContext.floor = &floor;
		collisionContext.stageMapRuntime = &runtime;
		collisionWorld.Rebuild(collisionContext);
		const bool entersSolidWorld = collisionWorld.GetSolidObbs().size() == 2;
		if (entersSolidWorld) {
			// 追加モデルを回転しても、Playerの接地・側面停止を同じ判定経路で確認します。
			levelData.objects[1].rotation.y = 0.785398f;
			runtime.ApplyEdits({ &levelData.objects[1] });
			collisionWorld.Rebuild(collisionContext);
			const MyMath::OBB& modelObb = collisionWorld.GetSolidObbs()[1];
			constexpr float kProbeRadius = 0.5f;
			const Vector3 topStart = Add(
				modelObb.center,
				{ 0.0f, modelObb.size.y + kProbeRadius - 0.05f, 0.0f });
			Player collisionProbe;
			if (collisionProbe.Initialize(object3dCommon, "debug/sphere.obj", topStart, kProbeRadius)) {
				const std::vector<MyMath::OBB> modelOnly{ modelObb };
				collisionProbe.UpdateWithControl(1.0f / 60.0f, modelOnly, { 0.0f, 0.0f, 1.0f }, {});
				const bool landed = collisionProbe.IsGrounded() && collisionProbe.IsColliding() &&
					collisionProbe.GetPosition().y >= topStart.y;
				const Vector3 sideStart = Add(
					modelObb.center,
					Multiply(modelObb.size.x + kProbeRadius - 0.08f, modelObb.orientations[0]));
				collisionProbe.SetPosition(sideStart);
				collisionProbe.UpdateWithControl(1.0f / 60.0f, modelOnly, { 0.0f, 0.0f, 1.0f }, {});
				const Vector3 sidePush = Subtract(collisionProbe.GetPosition(), sideStart);
				const bool stoppedAtSide = collisionProbe.IsColliding() && !collisionProbe.IsGrounded() &&
					Dot(sidePush, modelObb.orientations[0]) > 0.02f;
				const Vector3 farStart = Add(
					modelObb.center,
					Multiply(modelObb.size.x + kProbeRadius + 2.0f, modelObb.orientations[0]));
				collisionProbe.SetPosition(farStart);
				collisionProbe.UpdateWithControl(1.0f / 60.0f, modelOnly, { 0.0f, 0.0f, 1.0f }, {});
				playerCollision = landed && stoppedAtSide && !collisionProbe.IsColliding();
			}
		}
		levelData.objects[1].hasCollider = false;
		levelData.objects[1].collider = {};
		levelData.objects[1].objectType = "CARRYABLE_MIRROR";
		const bool rejectsCarryableMirror = !editor.AddBoxCollider(context) &&
			!levelData.objects[1].hasCollider;
		levelData.objects[1].objectType = "STATIC";
		context.applyLevelData = [](bool) { return false; };
		const bool renameRollback = !editor.RenameSelectedObject(context, "RejectedName") &&
			levelData.objects[1].name == "HallFloor";
		const bool colliderRollback = !editor.AddBoxCollider(context) &&
			!levelData.objects[1].hasCollider;
		LevelLoader::ObjectData reservedName{};
		reservedName.name = "EditorModel1";
		levelData.objects.push_back(std::move(reservedName));
		context.applyLevelData = [](bool) { return true; };
		const bool uniqueAddedName = editor.AddModel(context, "debug/floor.obj") &&
			levelData.objects.back().name == "EditorModel3";
		return rejectedDuplicate && rejectedBlank && renamed && addedCollider && entersSolidWorld &&
			playerCollision &&
			rejectsCarryableMirror && renameRollback && colliderRollback && uniqueAddedName;
	}

	// 実Stageを触らず、JSONの採用失敗と別Stageへの切替失敗で旧データを保つか確認します。
	bool CheckStageMapRepositoryRollback()
	{
		StageMapRepository repository;
		std::string status;
		const bool loaded = repository.SelectAndReload(1, []() { return true; }, status);
		if (!loaded || !repository.GetLevelData()) {
			return false;
		}
		repository.StartWatchingCurrentStage();
		const LevelLoader::LevelData* originalData = repository.GetLevelData();
		const std::string originalPath = repository.GetJsonFilePath();
		int rejectedApplyCalls = 0;
		const bool rejectedApply = !repository.Reload(
			[&rejectedApplyCalls]()
			{
				++rejectedApplyCalls;
				return false;
			}, status) &&
			rejectedApplyCalls == 1 && repository.GetLevelData() == originalData;
		const bool rejectedStageSwitch = !repository.SelectAndReload(
			2, []() { return false; }, status) &&
			repository.GetStageId() == 1 && repository.GetLevelData() == originalData &&
			repository.GetJsonFilePath() == originalPath;
		const bool rejectedInvalidId = !repository.SelectAndReload(
			0, []() { return true; }, status) &&
			repository.GetStageId() == 1 && repository.GetLevelData() == originalData;
		return rejectedApply && rejectedStageSwitch && rejectedInvalidId;
	}
}

// 本編が所有する鏡・Player・固定設定を受け取り、配置に依存しない反射経路も自動確認します。
void Stage1GameplaySmoke::Initialize(const InitializeContext& context)
{
	player_ = context.player;
	carryableMirror_ = context.carryableMirror;
	fixedMirrors_ = context.fixedMirrors;
	mirrorFloor_ = context.mirrorFloor;
	laserOrigin_ = context.laserOrigin;
	laserDirection_ = context.laserDirection;
	laserCollisionRadius_ = context.laserCollisionRadius;
	doorLaserOrigin_ = context.doorLaserOrigin;
	doorLaserDirection_ = context.doorLaserDirection;
	doorSwitchPosition_ = context.doorSwitchPosition;
	doorSwitchRadius_ = context.doorSwitchRadius;
	largeMirrorBaseYaw_ = context.largeMirrorBaseYaw;
	largeMirrorTargetYawOffset_ = context.largeMirrorTargetYawOffset;
	gameplaySmokeEnabled_ = IsEnvironmentEnabled("RIBONIN_CHALLENGE_STAGE1_GAMEPLAY_SMOKE");
	if (!gameplaySmokeEnabled_ || !player_ || !carryableMirror_ || !fixedMirrors_ ||
		!context.object3dCommon) {
		return;
	}

	VerifyColliderAndEditorContracts(context.object3dCommon);
	// 演出の更新値は独立Playerで確認し、実プレイ中の移動や死亡回数を変えません。
	gameplaySmokeLightDeathState_ = CheckStageLightDeath();
	// 実際のStage配置やPlayerに影響させず、危険Lightの周期だけを確認します。
	gameplaySmokeHazardTiming_ = CheckHazardLightTiming();

	VerifyEnemyManagerOwnership(context.object3dCommon);

	const auto& fixedMirrors = *fixedMirrors_;

	const Vector3 mirrorPosition = VerifyCarryMirrorControls(context);

	VerifyFixedMirrorAndDoor(fixedMirrors);

	VerifyLaserBlockingAndDrop(mirrorPosition);

	VerifyCameraControls();
}

// 携帯鏡の拾う操作・反射面・水平操作を順に確認し、元の床位置を返します。
Vector3 Stage1GameplaySmoke::VerifyCarryMirrorControls(const InitializeContext& context)
{
	gameplaySmokeStartPosition_ = player_->GetPosition();
	gameplaySmokeStartY_ = gameplaySmokeStartPosition_.y;
	const Vector3 mirrorPosition = carryableMirror_->GetMirror().GetCenter();

	// 本編ではJSONに置かれた鏡から開始するため、床に置かれた状態で拾う処理を確認します。
	carryableMirror_->SetCarried(false);
	// 鏡の中心にPlayerがいる条件を渡し、Eキーと同じ拾う処理を直接確認します。
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, true);
	gameplaySmokePickedUpMirror_ = carryableMirror_->IsCarried();
	// 持った状態のままPlayer正面へ移動させてから、反射判定を行います。
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, false);

	// 携帯中の鏡へ正面から光を当て、衝突後に反射線分が作られることを確認します。
	const Mirror& carryMirror = carryableMirror_->GetMirror();
	const Vector3 mirrorNormal = carryMirror.GetNormal();
	Laser carryMirrorProbe;
	carryMirrorProbe.SetOrigin({
		carryMirror.GetCenter().x + mirrorNormal.x * 3.0f,
		carryMirror.GetCenter().y + mirrorNormal.y * 3.0f,
		carryMirror.GetCenter().z + mirrorNormal.z * 3.0f,
	});
	carryMirrorProbe.SetDirection({ -mirrorNormal.x, -mirrorNormal.y, -mirrorNormal.z });
	carryMirrorProbe.SetMaxDistance(8.0f);
	carryMirrorProbe.SetMaxReflectionCount(1);
	carryMirrorProbe.Update({ &carryMirror });
	const std::vector<LaserSegment>& probeSegments = carryMirrorProbe.GetSegments();
	gameplaySmokeCarryMirrorReflectedLaser_ =
		probeSegments.size() >= 2 && probeSegments.front().hitMirror;

	// 現行の危険Lightと鏡の配置に依存しないよう、鏡のごく近くから本編の反射経路へ一本だけ渡します。
	// Sceneのライト配置は変えず、鏡一覧・遮蔽物・反射回数を含む実経路を確認します。
	if (context.reflectHazardSegments) {
		const Vector3 hazardProbeStart{
			carryMirror.GetCenter().x + mirrorNormal.x * 0.25f,
			carryMirror.GetCenter().y + mirrorNormal.y * 0.25f,
			carryMirror.GetCenter().z + mirrorNormal.z * 0.25f,
		};
		const Vector3 hazardProbeEnd{
			carryMirror.GetCenter().x - mirrorNormal.x * 0.25f,
			carryMirror.GetCenter().y - mirrorNormal.y * 0.25f,
			carryMirror.GetCenter().z - mirrorNormal.z * 0.25f,
		};
		const std::vector<LaserSegment> hazardProbeSegments = context.reflectHazardSegments({
			{ hazardProbeStart, hazardProbeEnd, false, 0 },
		});
		gameplaySmokeHazardReflectionRoute_ =
			hazardProbeSegments.size() >= 2 && hazardProbeSegments.front().hitMirror;
	}

	// 表側とは逆から当てたLaserは、携帯Mirrorでは反射しないことを確認します。
	Laser carryMirrorBackfaceProbe;
	carryMirrorBackfaceProbe.SetOrigin({
		carryMirror.GetCenter().x - mirrorNormal.x * 3.0f,
		carryMirror.GetCenter().y - mirrorNormal.y * 3.0f,
		carryMirror.GetCenter().z - mirrorNormal.z * 3.0f,
	});
	carryMirrorBackfaceProbe.SetDirection(mirrorNormal);
	carryMirrorBackfaceProbe.SetMaxDistance(8.0f);
	carryMirrorBackfaceProbe.SetMaxReflectionCount(1);
	carryMirrorBackfaceProbe.Update({ &carryMirror });
	gameplaySmokeCarryMirrorBackfaceIgnored_ =
		carryMirrorBackfaceProbe.GetSegments().size() == 1 &&
		!carryMirrorBackfaceProbe.GetSegments().front().hitMirror &&
		Length({
			carryMirrorBackfaceProbe.GetSegments().front().end.x - carryMirrorBackfaceProbe.GetSegments().front().start.x,
			carryMirrorBackfaceProbe.GetSegments().front().end.y - carryMirrorBackfaceProbe.GetSegments().front().start.y,
			carryMirrorBackfaceProbe.GetSegments().front().end.z - carryMirrorBackfaceProbe.GetSegments().front().start.z,
		}) < 3.10f;

	// Playerが右を向いた後は、鏡の中心と反射面がともに右前方へ移ることを確認します。
	carryableMirror_->Update(1.0f, mirrorPosition, 1.57079633f, false);
	const Vector3 turnedMirrorCenter = carryableMirror_->GetMirror().GetCenter();
	const Vector3 turnedMirrorNormal = carryableMirror_->GetMirror().GetNormal();
	gameplaySmokeCarryMirrorFollowsPlayerFacing_ =
		turnedMirrorCenter.x - mirrorPosition.x > 2.49f &&
		std::abs(turnedMirrorCenter.z - mirrorPosition.z) < 0.02f &&
		turnedMirrorNormal.x > 0.99f;
	// 後続の水平持ちテストは従来と同じ前方向から始めます。
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, false);

	// 短い右クリックを離す操作を二回行い、上向き・下向きの水平Mirrorになることを確認します。
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, true, 0.0f);
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	const bool isHorizontalMirrorFacingUp = carryableMirror_->GetMirror().GetNormal().y > 0.90f;
	const Vector3 horizontalMirrorOffset{
		carryableMirror_->GetMirror().GetCenter().x - mirrorPosition.x,
		carryableMirror_->GetMirror().GetCenter().y - mirrorPosition.y,
		carryableMirror_->GetMirror().GetCenter().z - mirrorPosition.z,
	};
	const bool isHorizontalMirrorFurtherForward =
		std::sqrt(horizontalMirrorOffset.x * horizontalMirrorOffset.x +
			horizontalMirrorOffset.z * horizontalMirrorOffset.z) > 2.50f;
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, true, 0.0f);
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	const bool isHorizontalMirrorFacingDown = carryableMirror_->GetMirror().GetNormal().y < -0.90f;
	// 下向きの水平Mirrorを前方へ傾けると、真下からのLaserがPlayer正面へ進むことを確認します。
	carryableMirror_->Update(0.50f, mirrorPosition, 0.0f, false, false, 0.0f, true, 100.0f);
	Laser carryMirrorTiltProbe;
	const Mirror& tiltedCarryMirror = carryableMirror_->GetMirror();
	carryMirrorTiltProbe.SetOrigin({
		tiltedCarryMirror.GetCenter().x,
		tiltedCarryMirror.GetCenter().y - 3.0f,
		tiltedCarryMirror.GetCenter().z,
	});
	carryMirrorTiltProbe.SetDirection({ 0.0f, 1.0f, 0.0f });
	carryMirrorTiltProbe.SetMaxDistance(8.0f);
	carryMirrorTiltProbe.SetMaxReflectionCount(1);
	carryMirrorTiltProbe.Update({ &tiltedCarryMirror });
	const std::vector<LaserSegment>& tiltedProbeSegments = carryMirrorTiltProbe.GetSegments();
	gameplaySmokeCarryMirrorTiltRedirectsLaser_ =
		tiltedProbeSegments.size() >= 2 &&
		tiltedProbeSegments.front().hitMirror &&
		tiltedProbeSegments[1].end.z - tiltedProbeSegments[1].start.z > 0.50f;
	// 長押しを離しても、短押し扱いになって表裏が切り替わらないことを確認します。
	carryableMirror_->Update(0.01f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	// 左クリックと同じ操作で、水平状態から通常の縦向きMirrorへ戻します。
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, false, true, 0.0f);
	// 長押しは鏡を反転させず、構えるための操作だけとして扱います。
	carryableMirror_->Update(0.30f, mirrorPosition, 0.0f, false, false, 0.0f, true, 0.0f);
	carryableMirror_->Update(0.01f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	const bool doesLongRightMouseHoldKeepVertical =
		std::abs(carryableMirror_->GetMirror().GetNormal().y) < 0.01f;
	gameplaySmokeCarryMirrorHorizontalControl_ =
		isHorizontalMirrorFacingUp &&
		isHorizontalMirrorFacingDown &&
		isHorizontalMirrorFurtherForward &&
		doesLongRightMouseHoldKeepVertical;
	return mirrorPosition;
}

// 携帯鏡による遮光と床へ戻す操作を確認し、通常Laserの遮光も検証します。
void Stage1GameplaySmoke::VerifyLaserBlockingAndDrop(const Vector3& mirrorPosition)
{
	const Mirror& carryMirror = carryableMirror_->GetMirror();
	const auto laserHitsSphere = [&](const Laser& testLaser, const Sphere& sphere) {
		return testLaser.IsHitSphere(sphere, laserCollisionRadius_);
	};
	// 実際に持っている鏡の現在位置と法線を使い、鏡なしなら当たる光が遮られることを確認します。
	const Vector3 carriedMirrorCenter = carryMirror.GetCenter();
	const Vector3 carriedMirrorNormal = carryMirror.GetNormal();
	const Sphere carriedPlayerSphere{
		{
			carriedMirrorCenter.x - carriedMirrorNormal.x * 2.4f,
			carriedMirrorCenter.y - carriedMirrorNormal.y * 2.4f,
			carriedMirrorCenter.z - carriedMirrorNormal.z * 2.4f,
		},
		1.2f,
	};
	const Vector3 carriedTestOrigin{
		carriedMirrorCenter.x + carriedMirrorNormal.x * 3.0f,
		carriedMirrorCenter.y + carriedMirrorNormal.y * 3.0f,
		carriedMirrorCenter.z + carriedMirrorNormal.z * 3.0f,
	};
	const Vector3 carriedTestDirection{
		-carriedMirrorNormal.x,
		-carriedMirrorNormal.y,
		-carriedMirrorNormal.z,
	};
	Laser carriedUnblockedLaser;
	carriedUnblockedLaser.SetOrigin(carriedTestOrigin);
	carriedUnblockedLaser.SetDirection(carriedTestDirection);
	carriedUnblockedLaser.SetMaxDistance(12.0f);
	carriedUnblockedLaser.Update({});
	Laser carriedBlockedLaser;
	carriedBlockedLaser.SetOrigin(carriedTestOrigin);
	carriedBlockedLaser.SetDirection(carriedTestDirection);
	carriedBlockedLaser.SetMaxDistance(12.0f);
	carriedBlockedLaser.SetMaxReflectionCount(1);
	carriedBlockedLaser.Update({ &carryMirror });
	gameplaySmokeCarriedMirrorBlocksPlayer_ =
		laserHitsSphere(carriedUnblockedLaser, carriedPlayerSphere) &&
		carriedBlockedLaser.GetSegments().size() >= 2 &&
		!laserHitsSphere(carriedBlockedLaser, carriedPlayerSphere);
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, true);
	gameplaySmokeDroppedMirror_ = !carryableMirror_->IsCarried();

	// 鏡がない時はPlayerへ届き、途中に鏡がある時は入射線が鏡で止まることを確認します。
	const Sphere testPlayerSphere{ { 0.0f, -0.8f, 5.0f }, 1.2f };
	Laser unblockedLaser;
	unblockedLaser.SetOrigin(laserOrigin_);
	unblockedLaser.SetDirection(laserDirection_);
	unblockedLaser.SetMaxDistance(30.0f);
	unblockedLaser.Update({});
	gameplaySmokeLaserHitsPlayer_ = laserHitsSphere(unblockedLaser, testPlayerSphere);

	const Vector3 normalizedLaserDirection = Normalize(laserDirection_);
	const Mirror shieldMirror(
		{
			laserOrigin_.x + normalizedLaserDirection.x * 1.2f,
			laserOrigin_.y + normalizedLaserDirection.y * 1.2f,
			laserOrigin_.z + normalizedLaserDirection.z * 1.2f,
		},
		Multiply(-1.0f, normalizedLaserDirection),
		3.0f,
		3.0f);
	Laser blockedLaser;
	blockedLaser.SetOrigin(laserOrigin_);
	blockedLaser.SetDirection(laserDirection_);
	blockedLaser.SetMaxDistance(30.0f);
	blockedLaser.SetMaxReflectionCount(1);
	blockedLaser.Update({ &shieldMirror });
	gameplaySmokeMirrorBlocksPlayer_ =
		blockedLaser.GetSegments().size() >= 2 &&
		!laserHitsSphere(blockedLaser, testPlayerSphere);
}

// Colliderと編集処理の契約を、Stage本編を変えずに検証します。
void Stage1GameplaySmoke::VerifyColliderAndEditorContracts(Engine::Object3dCommon* object3dCommon)
{
	// GameObjectを作らずに、Colliderだけで重なり・非重なり・無効状態を確認します。
	SphereCollider firstSphereCollider({ 0.0f, 0.0f, 0.0f }, 1.0f);
	SphereCollider overlappingSphereCollider({ 1.5f, 0.0f, 0.0f }, 1.0f);
	SphereCollider separatedSphereCollider({ 3.0f, 0.0f, 0.0f }, 1.0f);
	const bool detectsOverlap = firstSphereCollider.Check(overlappingSphereCollider).isCollision;
	const bool ignoresSeparatedCollider = !firstSphereCollider.Check(separatedSphereCollider).isCollision;
	overlappingSphereCollider.SetEnabled(false);
	const bool ignoresDisabledCollider = !firstSphereCollider.Check(overlappingSphereCollider).isCollision;
	ObbCollider overlappingObbCollider{};
	overlappingObbCollider.SetLocalShape({}, { 1.0f, 1.0f, 1.0f });
	overlappingObbCollider.SyncTransform({ { 1.0f, 1.0f, 1.0f }, {}, { 1.5f, 0.0f, 0.0f } });
	const Collision::CollisionInfo sphereToObbCollision =
		firstSphereCollider.Check(overlappingObbCollider);
	const Collision::CollisionInfo obbToSphereCollision =
		overlappingObbCollider.Check(firstSphereCollider);
	const bool detectsSphereObbOverlap = sphereToObbCollision.isCollision;
	// 呼び出す側を入れ替えても衝突し、押し出す法線だけ反対向きになります。
	const bool detectsObbSphereOverlap =
		obbToSphereCollision.isCollision &&
		Dot(sphereToObbCollision.normal, obbToSphereCollision.normal) < -0.99f;
	ObbCollider secondObbCollider{};
	secondObbCollider.SetLocalShape({}, { 1.0f, 1.0f, 1.0f });
	secondObbCollider.SyncTransform({ { 1.0f, 1.0f, 1.0f }, {}, { 2.5f, 0.0f, 0.0f } });
	const bool detectsObbObbOverlap =
		overlappingObbCollider.Check(secondObbCollider).isCollision;
	// Player利用側ではColliderの取り出しを省き、Player自身へ判定を依頼できます。
	ObbCollider playerProbeCollider{};
	playerProbeCollider.SetLocalShape({}, { 2.0f, 2.0f, 2.0f });
	playerProbeCollider.SyncTransform({
		{ 1.0f, 1.0f, 1.0f },
		{},
		player_->GetPosition(),
	});
	const bool playerChecksObbDirectly =
		player_->CheckCollision(playerProbeCollider).isCollision;
	// Enemyなどの球Colliderも、Playerから直接判定を呼べます。
	SphereCollider playerSphereProbeCollider{};
	playerSphereProbeCollider.SetShape(player_->GetPosition(), 1.0f);
	const bool playerChecksSphereDirectly =
		player_->CheckCollision(playerSphereProbeCollider).isCollision;
	gameplaySmokeSphereColliderApi_ =
		detectsOverlap &&
		ignoresSeparatedCollider &&
		ignoresDisabledCollider &&
		detectsSphereObbOverlap &&
		detectsObbSphereOverlap &&
		detectsObbObbOverlap &&
		playerChecksObbDirectly &&
		playerChecksSphereDirectly;
	gameplaySmokeStageMapColliderFlags_ = CheckStageMapColliderFlags();
	gameplaySmokeMirrorEditAtomicity_ = CheckMirrorEditsRejectPartialUpdate();
	gameplaySmokeStageEditorRollback_ = CheckStageEditorFailureRollback();
	gameplaySmokeStageEditorModelMetadata_ = CheckStageEditorModelMetadata(
		object3dCommon, gameplaySmokeStageEditorPlayerCollision_);
	gameplaySmokeStageEditorMirrorSync_ = CheckStageEditorMirrorSynchronization();
	gameplaySmokeStageMapRepositoryRollback_ = CheckStageMapRepositoryRollback();
}

// Enemyの生成と所有・解放をManagerへ任せられるか検証します。
void Stage1GameplaySmoke::VerifyEnemyManagerOwnership(Engine::Object3dCommon* object3dCommon)
{
	// StageがEnemyを直接newせず、Managerへ一覧生成と寿命管理を任せられるか確認します。
	EnemyManager enemyManager;
	enemyManager.Initialize(object3dCommon);
	const std::vector<Enemy::SpawnData> enemySpawnDataList{
		{ "debug/sphere.obj", { -2.0f, 0.0f, 0.0f }, 0.5f, false },
		{ "debug/sphere.obj", { 2.0f, 0.0f, 0.0f }, 0.5f, false },
	};
	const std::vector<Enemy*> spawnedEnemies =
		enemyManager.SpawnAll(enemySpawnDataList);
	// よく使う一体生成は、SpawnDataを組み立てずに短い引数だけで呼べます。
	Enemy* simpleSpawnedEnemy =
		enemyManager.Spawn("debug/sphere.obj", { 0.0f, 0.0f, 0.0f }, 0.5f);
	const bool hasExpectedSpawnedEnemies =
		spawnedEnemies.size() == 2 && simpleSpawnedEnemy != nullptr && enemyManager.GetCount() == 3 &&
		enemyManager.GetEnemy(0) == spawnedEnemies[0] &&
		enemyManager.GetEnemy(1) == spawnedEnemies[1] &&
		enemyManager.GetEnemy(2) == simpleSpawnedEnemy;
	// EnemyもPlayerと同じように、自分のColliderを意識せず判定を呼べます。
	if (hasExpectedSpawnedEnemies) {
		spawnedEnemies[1]->SetPosition({ -1.25f, 0.0f, 0.0f });
	}
	const bool enemyChecksEnemyDirectly =
		hasExpectedSpawnedEnemies &&
		spawnedEnemies[0]->CheckCollision(spawnedEnemies[1]->GetCollider()).isCollision;
	// 同じManagerを再利用しても、前のステージのEnemyを残さないことを確認します。
	enemyManager.Initialize(object3dCommon);
	const bool initializeClearsEnemies = enemyManager.GetCount() == 0;
	enemyManager.Clear();
	gameplaySmokeEnemyManagerSpawn_ =
		hasExpectedSpawnedEnemies && enemyChecksEnemyDirectly && initializeClearsEnemies &&
		enemyManager.GetCount() == 0;
}

// 鏡床とDoorの反射経路を、現在の固定鏡設定で検証します。
void Stage1GameplaySmoke::VerifyFixedMirrorAndDoor(const std::vector<std::unique_ptr<FixedMirror>>& fixedMirrors)
{
	// 鏡床は上から下へ向かうLightを、下から上へ反射することを確認します。
	if (mirrorFloor_) {
		Laser mirrorFloorProbe;
		const Mirror& mirrorFloor = mirrorFloor_->GetMirror();
		mirrorFloorProbe.SetOrigin({
			mirrorFloor.GetCenter().x,
			mirrorFloor.GetCenter().y + 3.0f,
			mirrorFloor.GetCenter().z,
		});
		mirrorFloorProbe.SetDirection({ 0.0f, -1.0f, 0.0f });
		mirrorFloorProbe.SetMaxDistance(8.0f);
		mirrorFloorProbe.SetMaxReflectionCount(1);
		mirrorFloorProbe.Update({ &mirrorFloor });
		Laser mirrorFloorBackfaceProbe;
		mirrorFloorBackfaceProbe.SetOrigin({
			mirrorFloor.GetCenter().x,
			mirrorFloor.GetCenter().y - 3.0f,
			mirrorFloor.GetCenter().z,
		});
		mirrorFloorBackfaceProbe.SetDirection({ 0.0f, 1.0f, 0.0f });
		mirrorFloorBackfaceProbe.SetMaxDistance(8.0f);
		mirrorFloorBackfaceProbe.SetMaxReflectionCount(1);
		mirrorFloorBackfaceProbe.Update({ &mirrorFloor });
		gameplaySmokeMirrorFloorReflectsLaser_ =
			mirrorFloorProbe.GetSegments().size() >= 2 &&
			mirrorFloorProbe.GetSegments().front().hitMirror &&
			mirrorFloorProbe.GetSegments()[1].end.y > mirrorFloorProbe.GetSegments()[1].start.y &&
			mirrorFloorBackfaceProbe.GetSegments().size() >= 2 &&
			mirrorFloorBackfaceProbe.GetSegments().front().hitMirror &&
			mirrorFloorBackfaceProbe.GetSegments()[1].end.y < mirrorFloorBackfaceProbe.GetSegments()[1].start.y;
	}

	// 斜め回転した大型MirrorがDoor Switchへ光を送れることを確認します。
	if (!fixedMirrors.empty() && fixedMirrors.front()) {
		const Mirror diagonalDoorMirror(
			fixedMirrors.front()->GetMirror().GetCenter(),
			{ std::sin(largeMirrorBaseYaw_ + largeMirrorTargetYawOffset_), 0.0f,
				std::cos(largeMirrorBaseYaw_ + largeMirrorTargetYawOffset_) },
			fixedMirrors.front()->GetMirror().GetWidth(),
			fixedMirrors.front()->GetMirror().GetHeight());
		Laser diagonalDoorLaser;
		diagonalDoorLaser.SetOrigin(doorLaserOrigin_);
		diagonalDoorLaser.SetDirection(doorLaserDirection_);
		diagonalDoorLaser.SetMaxDistance(20.0f);
		diagonalDoorLaser.SetMaxReflectionCount(1);
		diagonalDoorLaser.Update({ &diagonalDoorMirror });
		const Sphere diagonalDoorSwitch{ doorSwitchPosition_, doorSwitchRadius_ };
		gameplaySmokeDoorDiagonalReflection_ =
			diagonalDoorLaser.IsHitSphere(diagonalDoorSwitch, laserCollisionRadius_);
	}
}

// 同名登録や別名共有の変更時に、CameraManagerが選択中の登録を正しく維持するか確認します。
void Stage1GameplaySmoke::VerifyCameraRegistration(Engine::Camera& testCamera)
{
	// 同名再登録と別名共有の両方で、選択中の登録だけが切り替わることを確認します。
	Engine::Camera replacementCamera;
	Engine::CameraManager testCameraManager;
	testCameraManager.AddCamera("MainCamera", &testCamera);
	testCameraManager.AddCamera("MainCamera", &replacementCamera);
	gameplaySmokeCameraRegistrationReplacement_ =
		testCameraManager.GetActiveCamera() == &replacementCamera;
	testCameraManager.AddCamera("AliasCamera", &replacementCamera);
	testCameraManager.AddCamera("AliasCamera", &testCamera);
	gameplaySmokeCameraRegistrationReplacement_ =
		gameplaySmokeCameraRegistrationReplacement_ &&
		testCameraManager.GetActiveCamera() == &replacementCamera;
	testCameraManager.SetActiveCamera("AliasCamera");
	gameplaySmokeCameraRegistrationReplacement_ =
		gameplaySmokeCameraRegistrationReplacement_ &&
		testCameraManager.GetActiveCamera() == &testCamera;
	testCameraManager.AddCamera("AliasCamera", &replacementCamera);
	gameplaySmokeCameraRegistrationReplacement_ =
		gameplaySmokeCameraRegistrationReplacement_ &&
		testCameraManager.GetActiveCamera() == &replacementCamera;
	testCameraManager.RemoveCamera("MainCamera");
	gameplaySmokeCameraRegistrationReplacement_ =
		gameplaySmokeCameraRegistrationReplacement_ &&
		testCameraManager.GetActiveCamera() == &replacementCamera;
	testCameraManager.RemoveCamera("AliasCamera");
	gameplaySmokeCameraRegistrationReplacement_ =
		gameplaySmokeCameraRegistrationReplacement_ &&
		testCameraManager.GetActiveCamera() == nullptr;
}

// 描画用Cameraとは独立したControllerで、段階操作と壁制限を検証します。
void Stage1GameplaySmoke::VerifyCameraControls()
{
	// CameraManagerの登録を先に確認し、その後に同じCameraで操作を確認します。
	Engine::Camera testCamera;
	VerifyCameraRegistration(testCamera);
	Engine::CameraController testController;
	testController.Initialize(&testCamera, { 0.0f, 0.0f, 0.0f });
	testController.SetAutoRecenterEnabled(false);

	// 左一段目のCamera候補へ壁を置き、壁側へ回れないことを確認します。
	constexpr float testYaw = 0.52359878f;
	const float testPitch = testController.GetOrbitPitch();
	const float testDistance = testController.GetDistance();
	const Vector3 testFocus = testController.GetFocus();
	const float horizontalDistance = testDistance * std::cos(testPitch);
	const Vector3 blockedCameraPosition{
		testFocus.x - std::sin(testYaw) * horizontalDistance,
		testFocus.y + std::sin(testPitch) * testDistance,
		testFocus.z - std::cos(testYaw) * horizontalDistance,
	};
	OBB testWall{};
	testWall.center = Lerp(testFocus, blockedCameraPosition, 0.5f);
	testWall.orientations[0] = { 1.0f, 0.0f, 0.0f };
	testWall.orientations[1] = { 0.0f, 1.0f, 0.0f };
	testWall.orientations[2] = { 0.0f, 0.0f, 1.0f };
	testWall.size = { 0.75f, 0.75f, 0.75f };
	gameplaySmokeCameraWallBlock_ =
		!testController.TryStepOrbit(1, { testWall }) &&
		testController.GetOrbitStepIndex() == 0;

	// 壁越しシルエットを使うStageでは、壁があってもCameraをPlayer側へ縮めないことを確認します。
	Engine::Camera silhouetteCamera;
	Engine::CameraController silhouetteController;
	silhouetteController.Initialize(&silhouetteCamera, { 0.0f, 0.0f, 0.0f });
	silhouetteController.SetAutoRecenterEnabled(false);
	silhouetteController.SetWallAvoidanceEnabled(false);
	const Vector3 silhouetteTargetPosition = silhouetteController.GetCameraPosition();
	OBB silhouetteWall{};
	silhouetteWall.center = Lerp(
		silhouetteController.GetFocus(),
		silhouetteTargetPosition,
		0.5f);
	silhouetteWall.orientations[0] = { 1.0f, 0.0f, 0.0f };
	silhouetteWall.orientations[1] = { 0.0f, 1.0f, 0.0f };
	silhouetteWall.orientations[2] = { 0.0f, 0.0f, 1.0f };
	silhouetteWall.size = { 0.75f, 0.75f, 0.75f };
	silhouetteController.Update(
		1.0f / 60.0f,
		{ 0.0f, 0.0f, 0.0f },
		{},
		false,
		{ silhouetteWall });
	const Vector3 silhouettePositionDifference{
		silhouetteController.GetCameraPosition().x - silhouetteTargetPosition.x,
		silhouetteController.GetCameraPosition().y - silhouetteTargetPosition.y,
		silhouetteController.GetCameraPosition().z - silhouetteTargetPosition.z,
	};
	gameplaySmokeCameraKeepsDistanceBehindWall_ =
		Length(silhouettePositionDifference) < 0.0001f;

	// 一段目の直後も現在角度が目標へ瞬間移動せず、途中にあることを確認します。
	const float yawBeforeStep = testController.GetOrbitYaw();
	const bool firstOrbitStep = testController.TryStepOrbit(1, {});
	testController.Update(
		1.0f / 60.0f,
		{ 0.0f, 0.0f, 0.0f },
		{},
		true,
		{});
	const float yawAfterOneFrame = testController.GetOrbitYaw();
	gameplaySmokeCameraSmooth_ =
		firstOrbitStep &&
		yawAfterOneFrame > yawBeforeStep + 0.0001f &&
		yawAfterOneFrame < testYaw - 0.0001f;

	// PlayerがCamera側へ後退しても、Dead Zoneを越えたFocusがすぐ追いつくことを確認します。
	Engine::Camera backwardTestCamera;
	Engine::CameraController backwardTestController;
	backwardTestController.Initialize(&backwardTestCamera, { 0.0f, 0.0f, 0.0f });
	backwardTestController.SetAutoRecenterEnabled(false);
	backwardTestController.Update(
		0.25f,
		{ 0.0f, 0.0f, -8.0f },
		{ 0.0f, 0.0f, -1.0f },
		false,
		{});
	gameplaySmokeCameraBacktracks_ = backwardTestController.GetFocus().z < -5.0f;

	// 左右を十二回押すと、一回30度のまま360度回り切れることを確認します。
	Engine::Camera fullOrbitCamera;
	Engine::CameraController fullOrbitController;
	fullOrbitController.Initialize(&fullOrbitCamera, { 0.0f, 0.0f, 0.0f });
	fullOrbitController.SetAutoRecenterEnabled(false);
	bool canCompleteFullOrbit = true;
	for (int stepIndex = 0; stepIndex < 12; ++stepIndex) {
		canCompleteFullOrbit &= fullOrbitController.TryStepOrbit(1, {});
	}
	const bool canReturnFromFullOrbit =
		fullOrbitController.TryStepOrbit(-1, {}) &&
		fullOrbitController.GetOrbitStepIndex() == 11;
	const bool distanceReachedFarLimit =
		testController.TryStepDistance(1, {}) &&
		!testController.TryStepDistance(1, {}) &&
		testController.GetDistanceStepIndex() == 3;
	const bool distanceReachedNearLimit =
		testController.TryStepDistance(-1, {}) &&
		testController.TryStepDistance(-1, {}) &&
		testController.TryStepDistance(-1, {}) &&
		!testController.TryStepDistance(-1, {}) &&
		testController.GetDistanceStepIndex() == 0;
	gameplaySmokeCameraSteps_ =
		canCompleteFullOrbit &&
		canReturnFromFullOrbit &&
		distanceReachedFarLimit &&
		distanceReachedNearLimit;
}

// 自動確認が有効かを返し、Stage1が専用の入力制御を選ぶために使います。
bool Stage1GameplaySmoke::IsEnabled() const
{
	return gameplaySmokeEnabled_;
}

// 接地・反射Texture・危険Light・Doorの結果を、各フレームで先に蓄積します。
void Stage1GameplaySmoke::ObserveFrame(const UpdateContext& context, float deltaTime)
{
	const auto& fixedMirrors = *fixedMirrors_;

	++gameplaySmokeFrame_;
	gameplaySmokeElapsedTime_ += (std::max)(deltaTime, 0.0f);
	if (player_->IsGrounded()) {
		gameplaySmokeSawGrounded_ = true;
	}
	for (const auto& fixedMirror : fixedMirrors) {
		if (fixedMirror && fixedMirror->HasReflectionCapture()) {
			gameplaySmokeFixedMirrorReflection_ = true;
			break;
		}
	}
	if (mirrorFloor_ && mirrorFloor_->HasReflectionCapture()) {
		gameplaySmokeMirrorFloorReflection_ = true;
	}
	const auto hasReflectedSegment = [](const std::vector<LaserSegment>& segments) {
		return std::any_of(
			segments.begin(),
			segments.end(),
			[](const LaserSegment& segment) { return segment.hitMirror; });
	};
	gameplaySmokeHazardLightReflection_ =
		gameplaySmokeHazardLightReflection_ ||
		hasReflectedSegment(context.hazardLights->GetCeilingSweepSegments()) ||
		hasReflectedSegment(context.hazardLights->GetHorizontalMoveSegments()) ||
		hasReflectedSegment(context.hazardLights->GetBottomPulseSegments()) ||
		hasReflectedSegment(context.hazardLights->GetOrbitSegments());
	gameplaySmokeDoorStartsClosed_ =
		gameplaySmokeDoorStartsClosed_ ||
		(!context.isDoorSwitchReceivingLight && context.doorOpenAmount <= 0.01f);
}

// 置いた鏡の衝突登録を見てから、Playerへ一時的に持たせて向きの追従を確認します。
void Stage1GameplaySmoke::VerifyDroppedMirrorAndFacing(const UpdateContext& context)
{
	// 置いたMirrorのOBBが、実際にPlayerへ渡す障害物一覧へ含まれるかを確認します。
	// Collider単体の存在だけでなく、StageCollisionWorldへの登録漏れもここで検出できます。
	if (gameplaySmokeDroppedMirror_ && carryableMirror_ &&
		!carryableMirror_->IsCarried() && context.solidObbs) {
		const MyMath::OBB& droppedMirrorObb = carryableMirror_->GetObb();
		gameplaySmokeDroppedMirrorIsSolid_ = std::any_of(
			context.solidObbs->begin(),
			context.solidObbs->end(),
			[&droppedMirrorObb](const MyMath::OBB& solidObb)
			{
				const Vector3 centerDifference{
					solidObb.center.x - droppedMirrorObb.center.x,
					solidObb.center.y - droppedMirrorObb.center.y,
					solidObb.center.z - droppedMirrorObb.center.z,
				};
				const Vector3 sizeDifference{
					solidObb.size.x - droppedMirrorObb.size.x,
					solidObb.size.y - droppedMirrorObb.size.y,
					solidObb.size.z - droppedMirrorObb.size.z,
				};
				return Length(centerDifference) < 0.001f && Length(sizeDifference) < 0.001f;
			});
	}
	// 本編のStage1::UpdateMirrorGameplayを通し、横向きのPlayerへ鏡面と持ち位置が追従するかを確認します。
	// 確認後は元の床位置へ戻し、後続の落下確認へ影響させません。
	if (carryableMirror_ && !gameplaySmokeStageMirrorProbeStarted_ &&
		gameplaySmokeDroppedMirrorIsSolid_ && gameplaySmokeElapsedTime_ < 6.0f &&
		player_->GetFacingYaw() > 1.3f) {
		gameplaySmokeStageMirrorDropPosition_ = carryableMirror_->GetMirror().GetCenter();
		carryableMirror_->SetCarried(true);
		gameplaySmokeStageMirrorProbeStarted_ = true;
	} else if (carryableMirror_ && gameplaySmokeStageMirrorProbeStarted_ &&
		!gameplaySmokeStageMirrorProbeFinished_) {
		const float facingYaw = player_->GetFacingYaw();
		const Vector3 playerForward{ std::sin(facingYaw), 0.0f, std::cos(facingYaw) };
		const Mirror& heldMirror = carryableMirror_->GetMirror();
		const Vector3 heldOffset{
			heldMirror.GetCenter().x - player_->GetPosition().x,
			0.0f,
			heldMirror.GetCenter().z - player_->GetPosition().z,
		};
		const float heldDistance = Length(heldOffset);
		gameplaySmokeStageMirrorFollowsPlayerFacing_ =
			heldDistance > 0.001f &&
			Dot(heldOffset, playerForward) / heldDistance > 0.95f &&
			Dot(heldMirror.GetNormal(), playerForward) > 0.95f;
		if (gameplaySmokeStageMirrorFollowsPlayerFacing_ || gameplaySmokeElapsedTime_ >= 6.5f) {
			carryableMirror_->SetCarried(false);
			carryableMirror_->SetDroppedPosition(gameplaySmokeStageMirrorDropPosition_);
			gameplaySmokeStageMirrorProbeFinished_ = true;
		}
	}
}

// 鏡の追従検証後に一度だけ床外へ移し、Playerの接地解除と落下を記録します。
void Stage1GameplaySmoke::VerifyFallProbe()
{
	// 床の端へ歩かせる検証は、Stageの床形状が変わると成立しません。
	// 開始演出後に床から離れた位置へ一度だけ移し、重力と落下だけを独立して確認します。
	if (gameplaySmokeSawGrounded_ &&
		!gameplaySmokeStartedFallProbe_ &&
		gameplaySmokeElapsedTime_ >= 7.0f) {
		player_->SetPosition({
			gameplaySmokeStartPosition_.x + 20.0f,
			gameplaySmokeStartY_,
			gameplaySmokeStartPosition_.z,
		});
		gameplaySmokeStartedFallProbe_ = true;
	}
	if (gameplaySmokeStartedFallProbe_ && !player_->IsGrounded()) {
		gameplaySmokeLeftFloor_ = true;
	}
	if (gameplaySmokeLeftFloor_ &&
		player_->GetPosition().y < gameplaySmokeStartY_ - 1.0f) {
		gameplaySmokeFell_ = true;
	}
}

// Stage1の本編結果を順に確認し、終了条件を満たした時だけログを出力します。
void Stage1GameplaySmoke::Update(const UpdateContext& context, float deltaTime)
{
	if (!gameplaySmokeEnabled_ || !player_) {
		return;
	}

	if (!fixedMirrors_ || !context.hazardLights) {
		return;
	}
	ObserveFrame(context, deltaTime);
	VerifyDroppedMirrorAndFacing(context);
	VerifyFallProbe();
	FinishWhenReady();
}

// 成功条件か時間制限に達した時だけ全項目を判定し、今回の結果を記録します。
void Stage1GameplaySmoke::FinishWhenReady()
{
	const bool finishedSuccessfully =
		gameplaySmokeFell_ && gameplaySmokeElapsedTime_ >= 3.0f;
	const bool timedOut =
		gameplaySmokeElapsedTime_ >= kGameplaySmokeTimeoutSeconds ||
		gameplaySmokeFrame_ >= kGameplaySmokeMaximumFrameCount;
	if (!finishedSuccessfully && !timedOut) {
		return;
	}

	const bool success =
		gameplaySmokeSphereColliderApi_ &&
		gameplaySmokeStageMapColliderFlags_ &&
		gameplaySmokeLightDeathState_ &&
		gameplaySmokeHazardTiming_ &&
		gameplaySmokeMirrorEditAtomicity_ &&
		gameplaySmokeStageEditorRollback_ &&
		gameplaySmokeStageEditorModelMetadata_ &&
		gameplaySmokeStageEditorPlayerCollision_ &&
		gameplaySmokeStageEditorMirrorSync_ &&
		gameplaySmokeStageMapRepositoryRollback_ &&
		gameplaySmokeEnemyManagerSpawn_ &&
		gameplaySmokePickedUpMirror_ &&
		gameplaySmokeDroppedMirror_ &&
		gameplaySmokeDroppedMirrorIsSolid_ &&
		gameplaySmokeCarryMirrorReflectedLaser_ &&
		gameplaySmokeCarryMirrorBackfaceIgnored_ &&
		gameplaySmokeCarryMirrorFollowsPlayerFacing_ &&
		gameplaySmokeStageMirrorFollowsPlayerFacing_ &&
		gameplaySmokeCarryMirrorHorizontalControl_ &&
		gameplaySmokeCarryMirrorTiltRedirectsLaser_ &&
		gameplaySmokeCarriedMirrorBlocksPlayer_ &&
		gameplaySmokeLaserHitsPlayer_ &&
		gameplaySmokeMirrorBlocksPlayer_ &&
		gameplaySmokeFixedMirrorReflection_ &&
		gameplaySmokeMirrorFloorReflectsLaser_ &&
		gameplaySmokeMirrorFloorReflection_ &&
		gameplaySmokeHazardReflectionRoute_ &&
		gameplaySmokeDoorStartsClosed_ &&
		gameplaySmokeDoorDiagonalReflection_ &&
		gameplaySmokeCameraRegistrationReplacement_ &&
		gameplaySmokeCameraSteps_ &&
		gameplaySmokeCameraWallBlock_ &&
		gameplaySmokeCameraKeepsDistanceBehindWall_ &&
		gameplaySmokeCameraSmooth_ &&
		gameplaySmokeCameraBacktracks_ &&
		gameplaySmokeSawGrounded_ &&
		gameplaySmokeLeftFloor_ &&
		gameplaySmokeFell_;
	std::ofstream log("logs/stage1_gameplay_smoke.log", std::ios::trunc);
	if (log) {
		log << (success ? "SUCCESS" : "FAILURE")
			<< ": sphereColliderApi=" << gameplaySmokeSphereColliderApi_
			<< " stageMapColliderFlags=" << gameplaySmokeStageMapColliderFlags_
			<< " lightDeathState=" << gameplaySmokeLightDeathState_
			<< " hazardTiming=" << gameplaySmokeHazardTiming_
			<< " mirrorEditAtomicity=" << gameplaySmokeMirrorEditAtomicity_
			<< " stageEditorRollback=" << gameplaySmokeStageEditorRollback_
			<< " stageEditorModelMetadata=" << gameplaySmokeStageEditorModelMetadata_
			<< " stageEditorPlayerCollision=" << gameplaySmokeStageEditorPlayerCollision_
			<< " stageEditorMirrorSync=" << gameplaySmokeStageEditorMirrorSync_
			<< " stageMapRepositoryRollback=" << gameplaySmokeStageMapRepositoryRollback_
			<< " enemyManager=" << gameplaySmokeEnemyManagerSpawn_
			<< " picked=" << gameplaySmokePickedUpMirror_
			<< " dropped=" << gameplaySmokeDroppedMirror_
			<< " droppedMirrorSolid=" << gameplaySmokeDroppedMirrorIsSolid_
			<< " carryLaser=" << gameplaySmokeCarryMirrorReflectedLaser_
			<< " carryBackface=" << gameplaySmokeCarryMirrorBackfaceIgnored_
			<< " carryFollowsPlayerFacing=" << gameplaySmokeCarryMirrorFollowsPlayerFacing_
			<< " stageMirrorFollowsPlayerFacing=" << gameplaySmokeStageMirrorFollowsPlayerFacing_
			<< " carryHorizontal=" << gameplaySmokeCarryMirrorHorizontalControl_
			<< " carryTiltLaser=" << gameplaySmokeCarryMirrorTiltRedirectsLaser_
			<< " carriedMirrorBlocksPlayer=" << gameplaySmokeCarriedMirrorBlocksPlayer_
			<< " laserHitsPlayer=" << gameplaySmokeLaserHitsPlayer_
			<< " mirrorBlocksPlayer=" << gameplaySmokeMirrorBlocksPlayer_
			<< " fixedMirrorReflection=" << gameplaySmokeFixedMirrorReflection_
			<< " mirrorFloorLaser=" << gameplaySmokeMirrorFloorReflectsLaser_
			<< " mirrorFloorReflection=" << gameplaySmokeMirrorFloorReflection_
			<< " hazardReflection=" << gameplaySmokeHazardLightReflection_
			<< " hazardReflectionRoute=" << gameplaySmokeHazardReflectionRoute_
			<< " doorStartsClosed=" << gameplaySmokeDoorStartsClosed_
			<< " doorDiagonal=" << gameplaySmokeDoorDiagonalReflection_
			<< " cameraRegistrationReplacement=" << gameplaySmokeCameraRegistrationReplacement_
			<< " cameraSteps=" << gameplaySmokeCameraSteps_
			<< " cameraWall=" << gameplaySmokeCameraWallBlock_
			<< " cameraKeepsDistance=" << gameplaySmokeCameraKeepsDistanceBehindWall_
			<< " cameraSmooth=" << gameplaySmokeCameraSmooth_
			<< " cameraBacktracks=" << gameplaySmokeCameraBacktracks_
			<< " grounded=" << gameplaySmokeSawGrounded_
			<< " fallProbe=" << gameplaySmokeStartedFallProbe_
			<< " leftFloor=" << gameplaySmokeLeftFloor_
			<< " fell=" << gameplaySmokeFell_
			<< " startPosition=" << gameplaySmokeStartPosition_.x
			<< ',' << gameplaySmokeStartPosition_.y
			<< ',' << gameplaySmokeStartPosition_.z
			<< " position=" << player_->GetPosition().x
			<< ',' << player_->GetPosition().y
			<< ',' << player_->GetPosition().z
			<< " seconds=" << gameplaySmokeElapsedTime_
			<< '\n';
	}
	gameplaySmokeEnabled_ = false;
	PostQuitMessage(success ? 0 : 1);
}
