#include "Stage1.h"

#include "FixedMirror.h"
#include "ImGuiManager.h"
#include "Player.h"
#include "debug/StagePuzzleDebugUi.h"
#include <algorithm>
#include <functional>

// Edit Viewでだけ、Levelの編集・保存・Collider確認UIをまとめて表示します。
void Stage1::DrawStageEditorUi()
{
	// Stage1はゲームデータを所有し、Edit Viewの表示・操作順は編集専用部品へ任せます。
	StageEditor::Context editorContext{};
	editorContext.isEditViewActive = ImGuiManager::GetInstance()->IsEditViewActive();
	editorContext.autoMapReload = &autoStageMapReload_;
	editorContext.mapFilePath = stageMapHotReload_.GetFilePath();
	editorContext.mapReloadStatus = &stageMapReloadStatus_;
	editorContext.levelData = stageMapData_.get();
	editorContext.playerPosition = player_ ? player_->GetPosition() : Vector3{};
	// P0がある時はJSONのPlayerStartを編集しても開始位置へ反映されないため、UIへ優先元を知らせます。
	editorContext.playerStartFromCsv = std::any_of(
		stageMapChipField_.GetChips().begin(), stageMapChipField_.GetChips().end(),
		[](const MapChipField::Chip& chip)
		{
			return MapChipField::GetType(chip) == MapChipType::PlayerStart && chip.subId == 0;
		});
	editorContext.selectedObjectIndex = &selectedStageMapObjectIndex_;
	editorContext.player = player_.get();
	editorContext.floorObb = floor_ ? &collisionWorld_.GetFloorObb() : nullptr;
	editorContext.floor = floor_;
	editorContext.activeCamera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
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
		ApplyStageLighting();
	};
	editorContext.saveLevelData = [this]()
	{
		return SaveStageMap();
	};
	editorContext.reloadMap = [this]()
	{
		ReloadStageMap();
		stageMapHotReload_.Synchronize();
	};
	stageEditor_.Draw(editorContext);
}

// Debug UIへ鏡・Light Puzzleの値を渡し、鏡が変わった時だけStage JSON用データへ同期します。
void Stage1::DrawStagePuzzleDebugUi()
{
	if (ImGuiManager::GetInstance()->IsEditViewActive()) {
		StagePuzzleDebugUi::MirrorContext mirrorContext{};
		mirrorContext.fixedMirror =
			!fixedMirrors_.empty() ? fixedMirrors_.front().get() : nullptr;
		mirrorContext.onMirrorChanged = [this]()
		{
			if (!stageMapData_ || fixedMirrors_.empty() || !fixedMirrors_.front()) {
				return;
			}
			FixedMirror& fixedMirror = *fixedMirrors_.front();
			Mirror& mirror = fixedMirror.GetMirror();
			// Puzzleが毎フレーム角度を計算するので、編集角度を元の設定にも戻します。
			// 進行中の回転量はJSONへ焼き込まず、基準角度だけを保存します。
			StageLightPuzzle::Settings& settings = lightPuzzle_.GetSettings();
			settings.largeMirrorBaseYaw = fixedMirror.GetYaw() -
				settings.largeMirrorTargetYawOffset * lightPuzzle_.GetLargeMirrorRotationAmountForEdit();
			std::function<bool(std::vector<LevelLoader::ObjectData>&)> syncMirrorData;
			syncMirrorData = [&](std::vector<LevelLoader::ObjectData>& objects)
			{
				for (LevelLoader::ObjectData& objectData : objects) {
					if (objectData.tag == "Mirror") {
						objectData.translation = mirror.GetCenter();
						objectData.rotation.y = settings.largeMirrorBaseYaw;
						objectData.scaling.x = mirror.GetWidth() * 0.5f;
						objectData.scaling.y = mirror.GetHeight() * 0.5f;
						stageMapReloadStatus_ = "Mirror edited in memory. Press Save Map to keep it.";
						return true;
					}
					if (syncMirrorData(objectData.children)) {
						return true;
					}
				}
				return false;
			};
			syncMirrorData(stageMapData_->objects);
		};
		StagePuzzleDebugUi::DrawMirror(mirrorContext);
	}

	if (!chargeSwitch_ || !doorSwitch_ || !lightDoor_) {
		return;
	}
	StagePuzzleDebugUi::LightContext lightContext{};
	StageLightPuzzle::Settings& puzzleSettings = lightPuzzle_.GetSettings();
	lightContext.carryableMirror = carryableMirror_.get();
	lightContext.laserOrigin = &puzzleSettings.laserOrigin;
	lightContext.laserDirection = &puzzleSettings.laserDirection;
	lightContext.doorLaserOrigin = &puzzleSettings.doorLaserOrigin;
	lightContext.doorLaserDirection = &puzzleSettings.doorLaserDirection;
	lightContext.laserVisualWidth = &puzzleSettings.laserVisualWidth;
	lightContext.chargeSwitchPosition = &puzzleSettings.chargeSwitchPosition;
	lightContext.doorSwitchPosition = &puzzleSettings.doorSwitchPosition;
	lightContext.largeMirrorTargetYawOffset = &puzzleSettings.largeMirrorTargetYawOffset;
	lightContext.isChargeSwitchReceivingLight = &lightPuzzle_.GetChargeSwitchReceivingLightForEdit();
	lightContext.mirrorCharge = &lightPuzzle_.GetMirrorChargeForEdit();
	lightContext.isLargeMirrorCharged = &lightPuzzle_.GetLargeMirrorChargedForEdit();
	lightContext.largeMirrorRotationAmount = &lightPuzzle_.GetLargeMirrorRotationAmountForEdit();
	lightContext.isDoorSwitchReceivingLight = &lightPuzzle_.GetDoorSwitchReceivingLightForEdit();
	lightContext.doorOpenAmount = &lightPuzzle_.GetDoorOpenAmountForEdit();
	lightContext.laserRenderer = lightPuzzle_.GetChargeLaserRenderer();
	lightContext.doorLaserRenderer = lightPuzzle_.GetDoorLaserRenderer();
	StagePuzzleDebugUi::DrawLightPuzzle(lightContext);
}
