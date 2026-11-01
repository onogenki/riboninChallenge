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
#include <algorithm>
#include <cstdio>
#include <numbers>

// Model Shelfの内容を初期化し、選択中StageのEdit Viewで使えるようにします。
void StageEditor::Initialize()
{
	levelEditor_.Initialize();
}

// Edit Viewで動かした一枚目の鏡だけを、入れ子のJSON Objectから探して保存用データへ写します。
bool StageEditor::SynchronizeFirstMirror(
	LevelLoader::LevelData& levelData, const FixedMirror& fixedMirror, float baseYaw)
{
	const Mirror& mirror = fixedMirror.GetMirror();
	std::function<bool(std::vector<LevelLoader::ObjectData>&)> syncMirrorData;
	syncMirrorData = [&](std::vector<LevelLoader::ObjectData>& objects)
	{
		for (LevelLoader::ObjectData& objectData : objects) {
			if (objectData.tag == "Mirror") {
				objectData.translation = mirror.GetCenter();
				objectData.rotation.y = baseYaw;
				objectData.scaling.x = mirror.GetWidth() * 0.5f;
				objectData.scaling.y = mirror.GetHeight() * 0.5f;
				SetReloadStatus("Mirror edited in memory. Press Save Map to keep it.");
				return true;
			}
			if (syncMirrorData(objectData.children)) {
				return true;
			}
		}
		return false;
	};
	return syncMirrorData(levelData.objects);
}

// Edit ViewだけでPlayer、Collider、Level、Viewportを順に表示し、最後にJSON編集を一手順として記録します。
void StageEditor::Draw(Context& context)
{
	if (!context.isEditViewActive) {
		return;
	}

	DrawPlayerControls(context);

	DrawCollision(context);
	if (!context.levelData) {
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
	bool isEditingContinuously = false;
#ifdef USE_IMGUI
	isEditingContinuously = ImGui::IsAnyItemActive() || editViewport_.IsDraggingTransform();
#endif
	editHistory_.Observe(*context.levelData, selectedObjectIndex_, isEditingContinuously);
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
					mapReloadStatus_ = "PlayerStart edited in memory. Press Save Map to keep it.";
				}
				if (ImGui::Button("Copy Current Player to PlayerStart")) {
					playerStart->translation = context.player->GetPosition();
					mapReloadStatus_ = "PlayerStart copied in memory. Press Save Map to keep it.";
				}
				ImGui::TextDisabled("Press Save Map, then restart the current stage to use this start position.");
			} else {
				ImGui::TextDisabled("No PlayerStart object exists in stage%d.json.", context.stageId);
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

// StageLevelEditorへ現在Stageのメモリと再構築操作だけを渡し、JSON保存はこの編集UIで受けます。
StageLevelEditor::Context StageEditor::MakeLevelEditorContext(Context& context)
{
	StageLevelEditor::Context levelEditorContext{};
	levelEditorContext.stageId = context.stageId;
	levelEditorContext.levelData = context.levelData;
	levelEditorContext.mapRuntime = context.mapRuntime;
	levelEditorContext.playerPosition = context.playerPosition;
	levelEditorContext.selectedObjectIndex = &selectedObjectIndex_;
	levelEditorContext.applyLevelData = context.applyLevelData;
	levelEditorContext.setStatus = [this](const std::string& message)
	{
		mapReloadStatus_ = message;
	};
	return levelEditorContext;
}

// JSONの追加・削除・保存と履歴復元を受け、失敗時に現在Stageを維持して編集を続けます。
bool StageEditor::DrawLevelControls(
	Context& context,
	const StageLevelEditor::Context& levelEditorContext)
{
	auto setStatus = [this](const std::string& message)
	{
		mapReloadStatus_ = message;
	};

	const LevelEditorResult levelEditorResult =
		ImGuiManager::GetInstance()->LevelHotReloadWindow(
			autoMapReload_,
			context.mapFilePath,
			mapReloadStatus_,
			context.levelData,
			selectedObjectIndex_,
			editHistory_.CanUndo(),
			editHistory_.CanRedo());
	if (levelEditorResult.undoRequested || levelEditorResult.redoRequested) {
		const auto apply = [&context]()
		{
			return context.applyLevelData && context.applyLevelData(true);
		};
		const bool restored = levelEditorResult.undoRequested
			? editHistory_.Undo(*context.levelData, selectedObjectIndex_, apply)
			: editHistory_.Redo(*context.levelData, selectedObjectIndex_, apply);
		if (restored) {
			ClampSelectedObjectIndex(context.levelData->objects.size());
			editViewport_.Finalize();
			setStatus(levelEditorResult.undoRequested
				? "Undid one Stage map edit in memory. Press Save Map to keep it."
				: "Redid one Stage map edit in memory. Press Save Map to keep it.");
		} else {
			setStatus("History restore failed. Current Stage map was kept.");
		}
		return true;
	}
	if (levelEditorResult.reloadRequested) {
		if (context.reloadMap) {
			context.reloadMap();
		}
		// 成功時はcontext.levelDataが古くなるため、LightingとViewportをこのフレームでは開きません。
		return true;
	} else {
		// 名前と判定の追加はStageLevelEditorが元データ・Runtime反映・失敗時復元をまとめます。
		if (levelEditorResult.renameRequested) {
			levelEditor_.RenameSelectedObject(levelEditorContext, levelEditorResult.requestedName);
		}
		if (levelEditorResult.addBoxColliderRequested) {
			levelEditor_.AddBoxCollider(levelEditorContext);
		}
		if (levelEditorResult.dataChanged) {
			// 値だけ変わって画面へ反映できなかった時も、成功と誤認しないよう理由を表示します。
			setStatus(context.applyLevelData && context.applyLevelData(false)
				? "Edited in memory. Press Save Map to keep it."
				: "Edit could not be applied. Check required Floor/Mirror data.");
		}
		if (levelEditorResult.addSphereRequested) {
			levelEditor_.AddSphere(levelEditorContext);
		}
		if (levelEditorResult.addEventPairRequested) {
			levelEditor_.AddEventPair(levelEditorContext);
		}
		if (levelEditorResult.addCameraAreaRequested) {
			levelEditor_.AddCameraArea(levelEditorContext);
		}
		if (levelEditorResult.addPathBoxRequested) {
			levelEditor_.AddPathBox(levelEditorContext);
		}
		if (levelEditorResult.removeSelectedRequested) {
			levelEditor_.RemoveSelectedObject(levelEditorContext);
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
			setStatus("Lighting changed in data, but could not be applied to the current stage.");
		}
	}
	return false;

}

// Viewport専用の選択状態を使い、JSON由来モデルのTransformを編集します。
void StageEditor::DrawViewport(Context& context)
{
	if (!context.levelData || !context.mapRuntime) {
		return;
	}

	StageEditViewport::Context viewportContext{};
	viewportContext.levelData = context.levelData;
	viewportContext.camera = context.activeCamera;
	viewportContext.floor = context.floor;
	viewportContext.carryableMirror = context.carryableMirror;
	viewportContext.fixedMirrors = context.fixedMirrors;
	viewportContext.mapRuntime = context.mapRuntime;
	viewportContext.selectedObjectIndex = &selectedObjectIndex_;
	viewportContext.applyEdits = [applyLevelData = context.applyLevelData]()
	{
		return applyLevelData && applyLevelData(false);
	};
	viewportContext.setStatus = [this](const std::string& message)
	{
		mapReloadStatus_ = message;
	};
	editViewport_.Draw(viewportContext);
}

// 再読込でJSONの個数が変わっても、Edit Viewが範囲外のオブジェクトを参照しないようにします。
void StageEditor::ClampSelectedObjectIndex(std::size_t objectCount)
{
	selectedObjectIndex_ = objectCount == 0
		? -1
		: std::clamp(selectedObjectIndex_, 0, static_cast<int>(objectCount) - 1);
}

// Stageを切り替えた後のUIが前Stageの選択と履歴を使わないよう、新しいJSONから始めます。
void StageEditor::ResetForStage(const LevelLoader::LevelData& levelData)
{
	selectedObjectIndex_ = levelData.objects.empty() ? -1 : 0;
	editViewport_.Finalize();
	levelEditor_.ResetForStage();
	ResetHistory(levelData);
}

// 外部保存による再読込では履歴の基準を新しいJSONへ替え、選択状態は保持します。
void StageEditor::ResetHistory(const LevelLoader::LevelData& levelData)
{
	editHistory_.Clear();
	editHistory_.Observe(levelData, selectedObjectIndex_, false);
}

// Stage1終了時にModel Shelf・Viewport・履歴が持つ前Sceneの状態を破棄します。
void StageEditor::Finalize()
{
	editViewport_.Finalize();
	levelEditor_.Finalize();
	editHistory_.Clear();
}
