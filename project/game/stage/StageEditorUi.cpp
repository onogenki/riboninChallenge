#include "Stage.h"

#include "FixedMirror.h"
#include "ImGuiManager.h"
#include "Player.h"
#include "debug/StagePuzzleDebugUi.h"
#include <algorithm>

// Edit Viewでだけ、Levelの編集・保存・Collider確認UIをまとめて表示します。
void Stage::DrawStageEditorUi()
{
	// Stage1はゲームデータを所有し、Edit Viewの表示・操作順は編集専用部品へ任せます。
	StageEditor::Context editorContext{};
	editorContext.isEditViewActive = ImGuiManager::GetInstance()->IsEditViewActive();
	editorContext.mapFilePath = stageMapRepository_.GetJsonFilePath();
	editorContext.levelData = stageMapRepository_.GetLevelData();
	editorContext.playerPosition = player_ ? player_->GetPosition() : Vector3{};
	// P0がある時はJSONのPlayerStartを編集しても開始位置へ反映されないため、UIへ優先元を知らせます。
	editorContext.playerStartFromCsv = std::any_of(
		stageMapRepository_.GetMapChipField().GetChips().begin(),
		stageMapRepository_.GetMapChipField().GetChips().end(),
		[](const MapChipField::Chip& chip)
		{
			return MapChipField::GetType(chip) == MapChipType::PlayerStart && chip.subId == 0;
		});
	editorContext.player = player_.get();
	editorContext.floorObb = floor_ ? &collisionWorld_.GetFloorObb() : nullptr;
	editorContext.floor = floor_;
	editorContext.activeCamera = cameraManager_ ? cameraManager_->GetActiveCamera() : nullptr;
	// 通常Cameraの所有者はStage1のまま、Edit Viewへ一時調整用の参照だけを渡します。
	editorContext.cameraController = cameraController_.get();
	editorContext.fixedMirrors = &fixedMirrors_;
	editorContext.mirrorFloor = mirrorFloor_.get();
	editorContext.carryableMirror = carryableMirror_.get();
	editorContext.mapRuntime = &stageMapRuntime_;
	editorContext.stageCameraEvents = &stageCameraEvents_;
	editorContext.applyLevelData = [this](bool rebuildRuntimeObjects)
	{
		return ApplyStageMapData(rebuildRuntimeObjects);
	};
	// 照明だけを変更した時は、置いた鏡や床をJSONの位置へ戻さずLightのみ更新します。
	editorContext.applyLighting = [this]()
	{
		const LevelLoader::LevelData* levelData = stageMapRepository_.GetLevelData();
		stageLighting_.ApplyLevel(levelData && levelData->hasLighting ? &levelData->lighting : nullptr);
	};
	editorContext.saveLevelData = [this]()
	{
		return SaveStageMap();
	};
	editorContext.reloadMap = [this]()
	{
		ReloadStageMap();
		stageMapRepository_.SynchronizeJson();
	};
	stageEditor_.Draw(editorContext);
}

// Debug UIへ鏡・Light Puzzleの値を渡し、鏡が変わった時だけStage JSON用データへ同期します。
void Stage::DrawStagePuzzleDebugUi()
{
	if (ImGuiManager::GetInstance()->IsEditViewActive()) {
		StagePuzzleDebugUi::MirrorContext mirrorContext{};
		mirrorContext.fixedMirror =
			!fixedMirrors_.empty() ? fixedMirrors_.front().get() : nullptr;
		mirrorContext.onMirrorChanged = [this]()
		{
			LevelLoader::LevelData* levelData = stageMapRepository_.GetLevelData();
			if (!levelData || fixedMirrors_.empty() || !fixedMirrors_.front()) {
				return;
			}
			FixedMirror& fixedMirror = *fixedMirrors_.front();
			// Puzzleが毎フレーム角度を計算するので、編集角度を元の設定にも戻します。
			// 進行中の回転量はJSONへ焼き込まず、基準角度だけを保存します。
			StageLightPuzzle::Settings settings = lightPuzzle_.GetSettings();
			settings.largeMirrorBaseYaw = fixedMirror.GetYaw() -
				settings.largeMirrorTargetYawOffset * lightPuzzle_.GetLargeMirrorRotationAmount();
			lightPuzzle_.SetSettings(settings);
			stageEditor_.SynchronizeFirstMirror(*levelData, fixedMirror, settings.largeMirrorBaseYaw);
		};
		StagePuzzleDebugUi::DrawMirror(mirrorContext);
	}

	if (!chargeSwitch_ || !doorSwitch_ || !lightDoor_) {
		return;
	}
	StagePuzzleDebugUi::LightContext lightContext{};
	StageLightPuzzle::Settings puzzleSettings = lightPuzzle_.GetSettings();
	lightContext.carryableMirror = carryableMirror_.get();
	// 編集対象はSettingsだけ、進行状態は表示用の値だけをUIへ渡します。
	lightContext.settings = &puzzleSettings;
	lightContext.isChargeSwitchReceivingLight = lightPuzzle_.IsChargeSwitchReceivingLight();
	lightContext.mirrorCharge = lightPuzzle_.GetMirrorCharge();
	lightContext.isLargeMirrorCharged = lightPuzzle_.IsLargeMirrorCharged();
	lightContext.largeMirrorRotationAmount = lightPuzzle_.GetLargeMirrorRotationAmount();
	lightContext.isDoorSwitchReceivingLight = lightPuzzle_.IsDoorSwitchReceivingLight();
	lightContext.doorOpenAmount = lightPuzzle_.GetDoorOpenAmount();
	// UIが実際に設定を変更した時だけ、Puzzleと所有Rendererへまとめて反映します。
	if (StagePuzzleDebugUi::DrawLightPuzzle(lightContext)) {
		lightPuzzle_.SetSettings(puzzleSettings);
	}
}
