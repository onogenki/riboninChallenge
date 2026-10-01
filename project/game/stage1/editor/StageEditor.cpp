#include "StageEditor.h"

#include "Camera.h"
#include "CameraController.h"
#include "CarryableMirror.h"
#include "FixedMirror.h"
#include "ImGuiManager.h"
#include "Player.h"
#include "StageCameraEvents.h"
#include "StageMapRuntime.h"
#include "StageMapObjectIndex.h"
#include "../debug/StagePuzzleDebugUi.h"
#include <cstdio>
#include <numbers>

// Model Shelfの内容を初期化し、Stage1のEdit Viewで使えるようにします。
void StageEditor::Initialize()
{
	levelEditor_.Initialize();
}

// Edit ViewだけでPlayer、Collider、Level、Viewportを順に表示し、保存対象を区別します。
void StageEditor::Draw(Context& context)
{
	if (!context.isEditViewActive) {
		return;
	}

	DrawPlayerControls(context);

	DrawCollision(context);
	if (!context.autoMapReload || !context.mapReloadStatus || !context.levelData ||
		!context.selectedObjectIndex) {
		return;
	}

	const StageLevelEditor::Context levelEditorContext =
		MakeLevelEditorContext(context);
	if (DrawLevelControls(context, levelEditorContext)) {
		// 再読込後のLevelDataはStage1から次フレームに借り直します。
		return;
	}
	DrawViewport(context);
	// Viewportを先に描画してからModel ShelfとDropを処理し、従来のUI順序を維持します。
	levelEditor_.DrawModelShelf(levelEditorContext);
	levelEditor_.HandleShelfDropOnEditView(levelEditorContext);
}

// 現在位置と通常Cameraの目標角度は即時反映、PlayerStartはJSON保存後の次回開始位置として編集します。
void StageEditor::DrawPlayerControls(Context& context)
{
#ifdef USE_IMGUI
	if (!context.player) {
		return;
	}
	// 既存のInspectorに収め、Stage Lightingの背後へ隠れて編集できなくなるのを防ぎます。
	const unsigned int inspectorDockId = ImGuiManager::GetInstance()->GetInspectorDockId();
	if (inspectorDockId != 0) {
		ImGui::SetNextWindowDockID(inspectorDockId, ImGuiCond_Always);
	}
	if (ImGui::Begin("Stage Player")) {
		ImGui::SeparatorText("Current Player (this run)");
		Vector3 position = context.player->GetPosition();
		if (ImGui::DragFloat3("Player Position", &position.x, 0.05f)) {
			// Player自身に依頼し、見た目・速度・Colliderを同時に更新します。
			context.player->SetPosition(position);
			context.playerPosition = position;
		}
		ImGui::TextDisabled("Current position is not saved. Restart restores PlayerStart.");
		if (context.cameraController) {
			// Edit Viewで通常Cameraの目標角度を数値指定し、Game Viewへ戻した時の壁裏を確認します。
			float orbitYawDegrees = context.cameraController->GetTargetOrbitYaw() *
				180.0f / std::numbers::pi_v<float>;
			if (ImGui::DragFloat("Game Camera Yaw (deg)", &orbitYawDegrees, 1.0f, -360.0f, 360.0f)) {
				context.cameraController->SetOrbitYaw(
					orbitYawDegrees * std::numbers::pi_v<float> / 180.0f);
			}
			ImGui::TextDisabled("This run only. Switch to Game View to see the camera move.");
		}

		ImGui::SeparatorText("PlayerStart (next run)");
		if (context.playerStartFromCsv) {
			ImGui::TextDisabled("stage1.csv has P0, so P0 takes priority over JSON PlayerStart.");
		} else if (context.levelData) {
			LevelLoader::ObjectData* playerStart =
				StageMapObjectIndex::FindPlayerStartForEdit(*context.levelData);
			if (playerStart) {
				Vector3 startPosition = playerStart->translation;
				if (ImGui::DragFloat3("PlayerStart Position", &startPosition.x, 0.05f)) {
					playerStart->translation = startPosition;
					if (context.mapReloadStatus) {
						*context.mapReloadStatus = "PlayerStart edited in memory. Press Save Map to keep it.";
					}
				}
				if (ImGui::Button("Copy Current Player to PlayerStart")) {
					playerStart->translation = context.player->GetPosition();
					if (context.mapReloadStatus) {
						*context.mapReloadStatus = "PlayerStart copied in memory. Press Save Map to keep it.";
					}
				}
				ImGui::TextDisabled("Press Save Map, then restart Stage1 to use this start position.");
			} else {
				ImGui::TextDisabled("No PlayerStart object exists in stage1.json.");
			}
		}
		if (context.carryableMirror) {
			DrawMirrorHoldControls(*context.carryableMirror);
		}
	}
	ImGui::End();
#else
	(void)context;
#endif
}

// 持ち位置だけをMirrorへ即時反映し、調整した値を初期設定へ戻しやすくします。
void StageEditor::DrawMirrorHoldControls(CarryableMirror& mirror)
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Carried Mirror (this run)");
	CarryableMirror::HoldSettings settings = mirror.GetHoldSettings();
	bool changed = false;
	changed |= ImGui::DragFloat("Vertical Distance", &settings.verticalDistance, 0.05f, 0.0f, 10.0f);
	changed |= ImGui::DragFloat("Vertical Side", &settings.verticalSide, 0.05f, -5.0f, 5.0f);
	changed |= ImGui::DragFloat("Vertical Height", &settings.verticalHeight, 0.05f, -2.0f, 5.0f);
	changed |= ImGui::DragFloat("Horizontal Distance", &settings.horizontalDistance, 0.05f, 0.0f, 10.0f);
	changed |= ImGui::DragFloat("Horizontal Height", &settings.horizontalHeight, 0.05f, -2.0f, 5.0f);
	if (changed) {
		mirror.SetHoldSettings(settings);
	}
	ImGui::TextDisabled("Changes apply while playing. Restart restores the C++ defaults.");
	if (ImGui::Button("Copy Mirror C++ Defaults")) {
		// CarryableMirror.hのHoldSettingsに、そのまま貼れる初期値だけをコピーします。
		char code[320]{};
		std::snprintf(code, sizeof(code),
			"// CarryableMirror.h の HoldSettings に設定する、携帯鏡の持ち位置です。\n"
			"float verticalDistance = %.3ff;\n"
			"float horizontalDistance = %.3ff;\n"
			"float verticalSide = %.3ff;\n"
			"float verticalHeight = %.3ff;\n"
			"float horizontalHeight = %.3ff;",
			settings.verticalDistance,
			settings.horizontalDistance,
			settings.verticalSide,
			settings.verticalHeight,
			settings.horizontalHeight);
		ImGui::SetClipboardText(code);
	}
#else
	(void)mirror;
#endif
}

// Collider確認用の表示に必要なStage1所有データを、Debug UI部品へ渡します。
void StageEditor::DrawCollision(const Context& context) const
{
	if (!context.player || !context.floorObb || !context.activeCamera ||
		!context.fixedMirrors || !context.mapRuntime || !context.stageCameraEvents) {
		return;
	}

	StagePuzzleDebugUi::CollisionContext collisionContext{};
	collisionContext.player = context.player;
	collisionContext.floorObb = context.floorObb;
	collisionContext.activeCamera = context.activeCamera;
	collisionContext.fixedMirrors = context.fixedMirrors;
	collisionContext.mirrorFloor = context.mirrorFloor;
	collisionContext.carryableMirror = context.carryableMirror;
	collisionContext.stageMapRuntime = context.mapRuntime;
	collisionContext.stageCameraEvents = context.stageCameraEvents;
	StagePuzzleDebugUi::DrawCollision(collisionContext);
}

// StageLevelEditorへ、Stage1が所有するJSONと保存・再構築操作だけを渡します。
StageLevelEditor::Context StageEditor::MakeLevelEditorContext(Context& context) const
{
	StageLevelEditor::Context levelEditorContext{};
	levelEditorContext.levelData = context.levelData;
	levelEditorContext.playerPosition = context.playerPosition;
	levelEditorContext.selectedObjectIndex = context.selectedObjectIndex;
	levelEditorContext.applyLevelData = context.applyLevelData;
	levelEditorContext.saveLevelData = context.saveLevelData;
	levelEditorContext.setStatus = [&context](const std::string& message)
	{
		if (context.mapReloadStatus) {
			*context.mapReloadStatus = message;
		}
	};
	return levelEditorContext;
}

// JSONの追加・削除・保存と、Lighting編集後の実行中Stageへの反映を行います。
bool StageEditor::DrawLevelControls(
	Context& context,
	const StageLevelEditor::Context& levelEditorContext)
{
	auto setStatus = [&context](const std::string& message)
	{
		*context.mapReloadStatus = message;
	};

	const LevelEditorResult levelEditorResult =
		ImGuiManager::GetInstance()->LevelHotReloadWindow(
			*context.autoMapReload,
			context.mapFilePath,
			*context.mapReloadStatus,
			context.levelData,
			*context.selectedObjectIndex);
	if (levelEditorResult.reloadRequested) {
		if (context.reloadMap) {
			context.reloadMap();
		}
		// 成功時はcontext.levelDataが古くなるため、LightingとViewportをこのフレームでは開きません。
		return true;
	} else {
		if (levelEditorResult.dataChanged) {
			// 値だけ変わって画面へ反映できなかった時も、成功と誤認しないよう理由を表示します。
			setStatus(context.applyLevelData && context.applyLevelData(false)
				? "Edited in memory. Press Save Map to keep it."
				: "Edit could not be applied. Check required Floor/Mirror data.");
		}
		if (levelEditorResult.addSphereRequested && levelEditor_.AddSphere(levelEditorContext)) {
			context.saveLevelData();
		}
		if (levelEditorResult.addEventPairRequested && levelEditor_.AddEventPair(levelEditorContext)) {
			context.saveLevelData();
		}
		if (levelEditorResult.addCameraAreaRequested && levelEditor_.AddCameraArea(levelEditorContext)) {
			context.saveLevelData();
		}
		if (levelEditorResult.addPathSphereRequested && levelEditor_.AddPathSphere(levelEditorContext)) {
			context.saveLevelData();
		}
		if (levelEditorResult.removeSelectedRequested && levelEditor_.RemoveSelectedObject(levelEditorContext)) {
			context.saveLevelData();
		}
		if (levelEditorResult.saveRequested && context.saveLevelData) {
			context.saveLevelData();
		}
	}

	if (ImGuiManager::GetInstance()->StageLightingWindow(context.levelData->lighting)) {
		// Lightだけを反映します。Stage全体の再反映は置いた鏡の位置まで戻すため行いません。
		context.levelData->hasLighting = true;
		if (context.applyLighting) {
			context.applyLighting();
			setStatus("Lighting edited in memory. Press Save Map to keep it.");
		} else {
			setStatus("Lighting changed in data, but could not be applied to Stage1.");
		}
	}
	return false;

}

// Viewport専用の選択状態を使い、JSON由来モデルのTransformを編集します。
void StageEditor::DrawViewport(Context& context)
{
	if (!context.levelData || !context.mapRuntime || !context.selectedObjectIndex) {
		return;
	}

	StageEditViewport::Context viewportContext{};
	viewportContext.levelData = context.levelData;
	viewportContext.camera = context.activeCamera;
	viewportContext.floor = context.floor;
	viewportContext.carryableMirror = context.carryableMirror;
	viewportContext.fixedMirrors = context.fixedMirrors;
	viewportContext.mapRuntime = context.mapRuntime;
	viewportContext.selectedObjectIndex = context.selectedObjectIndex;
	viewportContext.applyEdits = [applyLevelData = context.applyLevelData]()
	{
		return applyLevelData && applyLevelData(false);
	};
	viewportContext.setStatus = [&context](const std::string& message)
	{
		if (context.mapReloadStatus) {
			*context.mapReloadStatus = message;
		}
	};
	editViewport_.Draw(viewportContext);
}

// Stage1終了時に、編集UIが保持するModel ShelfとViewportの選択状態を破棄します。
void StageEditor::Finalize()
{
	editViewport_.Finalize();
	levelEditor_.Finalize();
}
