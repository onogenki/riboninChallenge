#include "ImGuiManager.h"

#include <algorithm>
#include <numbers>

// 選択中StageのJSON再読込と配置物の編集を表示し、削除操作は対象の詳細欄へ置きます。
LevelEditorResult ImGuiManager::LevelHotReloadWindow(
	bool& autoReload,
	const std::string& filePath,
	const std::string& status,
	LevelLoader::LevelData* levelData,
	int& selectedObjectIndex,
	bool canUndo,
	bool canRedo)
{
	LevelEditorResult result{};
#ifdef USE_IMGUI
	if (inspectorDockId_ != 0) {
		ImGui::SetNextWindowDockID(inspectorDockId_, ImGuiCond_Always);
	}
	if (!ImGui::Begin("Inspector", &showModelWindow_)) {
		ImGui::End();
		return result;
	}

	// 保存先と再読込を独立した欄にまとめ、選択物の編集欄を探しやすくします。
	if (ImGui::CollapsingHeader("Map File", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::TextWrapped("%s", filePath.c_str());
		ImGui::Checkbox("Auto Reload", &autoReload);
		ImGui::TextDisabled("Watches changes saved by an external editor.");
		result.saveRequested = ImGui::Button("Save Map");
		result.reloadRequested = ImGui::Button("Reload Now");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Discards unsaved edits and reloads the JSON file.");
		}
		// 履歴がある側だけ黄色にし、狭いInspectorでもReloadの右へ二つの操作を収めます。
		ImGui::SameLine();
		if (canUndo) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.63f, 0.12f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.92f, 0.75f, 0.20f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.50f, 0.08f, 1.0f));
		}
		result.undoRequested = ImGui::Button("Undo") && canUndo;
		if (canUndo) {
			ImGui::PopStyleColor(3);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(canUndo
				? "Undo one Stage map edit in memory. Press Save Map to keep it."
				: "No Stage map edit to undo.");
		}
		ImGui::SameLine();
		if (canRedo) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.63f, 0.12f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.92f, 0.75f, 0.20f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.50f, 0.08f, 1.0f));
		}
		result.redoRequested = ImGui::Button("Redo") && canRedo;
		if (canRedo) {
			ImGui::PopStyleColor(3);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(canRedo
				? "Redo one undone Stage map edit in memory. Press Save Map to keep it."
				: "No undone Stage map edit to redo.");
		}
		ImGui::TextWrapped("Only Save Map writes the JSON. Unsaved edits disappear on stage or scene change and exit.");
	}
	ImGui::TextWrapped("Status: %s", status.c_str());

	ImGui::SeparatorText("Map Objects");
	if (!levelData || levelData->objects.empty()) {
		ImGui::TextDisabled("No editable map objects.");
	} else {
		std::vector<LevelLoader::ObjectData>& objects = levelData->objects;
		if (selectedObjectIndex < 0 ||
			selectedObjectIndex >= static_cast<int>(objects.size())) {
			selectedObjectIndex = 0;
		}

		auto findTriggerIndexForCamera = [&](const std::string& cameraName) {
			for (int index = 0; index < static_cast<int>(objects.size()); ++index) {
				if (objects[index].objectType == "EVENT_TRIGGER" &&
					objects[index].eventCameraName == cameraName) {
					return index;
				}
			}
			return -1;
		};
		if (objects[selectedObjectIndex].objectType == "EVENT_CAMERA") {
			const int triggerIndex = findTriggerIndexForCamera(objects[selectedObjectIndex].name);
			if (triggerIndex >= 0) {
				selectedObjectIndex = triggerIndex;
			}
		}

		auto makeObjectDisplayName = [&](int index) {
			const LevelLoader::ObjectData& data = objects[index];
			if (data.objectType == "EVENT_TRIGGER") {
				return std::string("Event Camera + Trigger: ") + data.name;
			}
			return data.name;
		};

		const std::string selectedName = makeObjectDisplayName(selectedObjectIndex);
		if (ImGui::BeginCombo("Selected Object", selectedName.c_str())) {
			for (int index = 0; index < static_cast<int>(objects.size()); ++index) {
				if (objects[index].objectType == "EVENT_CAMERA" &&
					findTriggerIndexForCamera(objects[index].name) >= 0) {
					continue;
				}

				const bool isSelected = index == selectedObjectIndex;
				const std::string displayName = makeObjectDisplayName(index);
				if (ImGui::Selectable(displayName.c_str(), isSelected)) {
					selectedObjectIndex = index;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		LevelLoader::ObjectData& objectData = objects[selectedObjectIndex];
		ImGui::SeparatorText("Object Details");
		// 狭いInspectorでも選択対象の名前を最後まで読み、判定先を取り違えないよう折り返します。
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.82f, 1.0f, 1.0f));
		ImGui::TextWrapped("Editing: %s", objectData.name.c_str());
		ImGui::PopStyleColor();
		ImGui::TextWrapped("Model: %s", objectData.fileName.empty() ? "(none)" : objectData.fileName.c_str());
		// Tagと削除を同じ行に置き、狭いInspectorでも対象名を確認してから押せるようにします。
		if (ImGui::BeginTable("ObjectTagActions", 2, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 72.0f);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextWrapped("Tag: %s", objectData.tag.empty() ? "(none)" : objectData.tag.c_str());
			ImGui::TableSetColumnIndex(1);
			const bool protectedObject = objectData.tag == "Floor" || objectData.tag == "Mirror";
			// 削除できるモデルを選んだ時だけ赤くし、それ以外は通常のボタン色で表示します。
			const bool highlightRemovableModel = !protectedObject && objectData.type == "MESH";
			ImGui::BeginDisabled(protectedObject);
			if (highlightRemovableModel) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.16f, 0.16f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.82f, 0.24f, 0.24f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.10f, 0.10f, 1.0f));
			}
			result.removeSelectedRequested = ImGui::Button("Remove", ImVec2(-1.0f, 0.0f));
			if (highlightRemovableModel) {
				ImGui::PopStyleColor(3);
			}
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				if (protectedObject) {
					ImGui::SetTooltip("Floor and Mirror cannot be removed.");
				} else {
					ImGui::SetTooltip("Remove %s. Press Save Map to keep this change.", objectData.name.c_str());
				}
			}
			ImGui::EndTable();
		}
		if (objectData.tag == "EditorAdded") {
			// 入力中の文字をフレーム間で保持し、確定後だけRuntimeの名前対応を作り直します。
			static int nameEditIndex = -1;
			static std::string originalName;
			static std::string originalFilePath;
			static char nameBuffer[128]{};
			if (nameEditIndex != selectedObjectIndex || originalName != objectData.name ||
				originalFilePath != filePath) {
				nameEditIndex = selectedObjectIndex;
				originalName = objectData.name;
				originalFilePath = filePath;
				strncpy_s(nameBuffer, objectData.name.c_str(), _TRUNCATE);
			}
			const bool enterPressed = ImGui::InputText("Object Name", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
			ImGui::SameLine();
			const bool renameClicked = ImGui::Button("Rename");
			if (enterPressed || renameClicked) {
				result.renameRequested = true;
				result.requestedName = nameBuffer;
			}
		}

		constexpr float kRadianToDegree = 180.0f / std::numbers::pi_v<float>;
		constexpr float kDegreeToRadian = std::numbers::pi_v<float> / 180.0f;
		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
			const char* positionLabel = objectData.objectType == "EVENT_TRIGGER"
				? "Trigger Position"
				: (objectData.objectType == "CAMERA_AREA" ? "Area Position" : "Position");
			result.dataChanged |= ImGui::DragFloat3(
				positionLabel,
				&objectData.translation.x,
				0.05f);

			float rotationDegrees[3]{
				objectData.rotation.x * kRadianToDegree,
				objectData.rotation.y * kRadianToDegree,
				objectData.rotation.z * kRadianToDegree,
			};
			const char* rotationLabel = objectData.objectType == "EVENT_TRIGGER"
				? "Trigger Rotation (deg)"
				: (objectData.objectType == "CAMERA_AREA" ? "Area Rotation (deg)" : "Rotation (deg)");
			if (ImGui::DragFloat3(rotationLabel, rotationDegrees, 1.0f)) {
				objectData.rotation = {
					rotationDegrees[0] * kDegreeToRadian,
					rotationDegrees[1] * kDegreeToRadian,
					rotationDegrees[2] * kDegreeToRadian,
				};
				result.dataChanged = true;
			}

			const char* scaleLabel = objectData.objectType == "EVENT_TRIGGER"
				? "Trigger Scale"
				: (objectData.objectType == "CAMERA_AREA" ? "Area Scale" : "Scale");
			result.dataChanged |= ImGui::DragFloat3(
				scaleLabel,
				&objectData.scaling.x,
				0.05f,
				0.01f,
				100.0f);
		}

		if (ImGui::CollapsingHeader("Collision", ImGuiTreeNodeFlags_DefaultOpen)) {
			// 対象名とモデルを判定操作の直前にも示し、別の配置物への誤追加を防ぎます。
			ImGui::TextWrapped("Target: %s", objectData.name.c_str());
			ImGui::TextWrapped("Model: %s", objectData.fileName.empty() ? "(none)" : objectData.fileName.c_str());
			const bool canAddSolidCollider = objectData.type == "MESH" && !objectData.fileName.empty() &&
				objectData.tag != "Mirror" && objectData.tag != "Floor" && objectData.tag != "PlayerStart" &&
				objectData.objectType != "CARRYABLE_MIRROR" && objectData.objectType != "EVENT_TRIGGER" &&
				objectData.objectType != "EVENT_CAMERA" && objectData.objectType != "CAMERA_AREA";
			if (!objectData.hasCollider && canAddSolidCollider) {
				ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "No collider: player passes through.");
				// 対象名を直上に示したまま、狭いInspectorへ収まる幅で操作を出します。
				if (ImGui::Button("Add Box Collider", ImVec2(-1.0f, 0.0f))) {
					result.addBoxColliderRequested = true;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Add a solid box to %s", objectData.name.c_str());
				}
			}
			if (objectData.objectType == "CARRYABLE_MIRROR") {
				ImGui::TextDisabled("The carryable mirror owns its collider outside this JSON field.");
			} else if (objectData.objectType == "EVENT_CAMERA") {
				ImGui::TextDisabled("Event cameras do not use a solid collider.");
			} else if (objectData.hasCollider) {
				if (objectData.objectType == "EVENT_TRIGGER" || objectData.objectType == "CAMERA_AREA") {
					ImGui::TextUnformatted("Detection area: not a solid wall.");
				} else if (objectData.collider.type == "BOX") {
					ImGui::TextColored(ImVec4(0.55f, 0.88f, 0.55f, 1.0f), "Box Collider enabled.");
				} else {
					ImGui::TextUnformatted("Collider data is present.");
				}
				if (objectData.collider.type == "BOX") {
					ImGui::TextDisabled("The box follows Position, Rotation, and Scale above.");
					result.dataChanged |= ImGui::DragFloat3(
						"Collider Center",
						&objectData.collider.center.x,
						0.05f);
					result.dataChanged |= ImGui::DragFloat3(
						"Collider Size",
						&objectData.collider.size.x,
						0.05f,
						0.01f,
						100.0f);
				} else {
					ImGui::TextDisabled("This collider type is not editable here: %s", objectData.collider.type.c_str());
				}
			} else if (!canAddSolidCollider) {
				ImGui::TextDisabled("No solid collider for this object.");
			}
		}

		if (objectData.objectType == "EVENT_TRIGGER") {
			ImGui::SeparatorText("Event Trigger");
			ImGui::TextDisabled("This box changes the camera. It is not a solid wall.");
			ImGui::Text(
				"Event ID: %s",
				objectData.eventId.empty() ? "(none)" : objectData.eventId.c_str());

			const char* currentCameraName = objectData.eventCameraName.empty()
				? "(none)"
				: objectData.eventCameraName.c_str();
			if (ImGui::BeginCombo("Event Camera", currentCameraName)) {
				for (const LevelLoader::ObjectData& cameraData : objects) {
					if (cameraData.objectType != "EVENT_CAMERA") {
						continue;
					}
					const bool isSelected =
						cameraData.name == objectData.eventCameraName;
					if (ImGui::Selectable(cameraData.name.c_str(), isSelected)) {
						objectData.eventCameraName = cameraData.name;
						result.dataChanged = true;
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			const auto cameraFound = std::find_if(
				objects.begin(),
				objects.end(),
				[&](const LevelLoader::ObjectData& cameraData)
				{
					return cameraData.objectType == "EVENT_CAMERA" &&
						cameraData.name == objectData.eventCameraName;
				});
			if (cameraFound != objects.end()) {
				LevelLoader::ObjectData& cameraData = *cameraFound;
				ImGui::SeparatorText("Event Camera View");
				ImGui::TextDisabled("Camera Position is where the camera is placed. Camera Focus is what it looks at.");
				result.dataChanged |= ImGui::DragFloat3(
					"Camera Position",
					&cameraData.translation.x,
					0.05f);
				if (!cameraData.hasCameraFocus) {
					cameraData.hasCameraFocus = true;
					cameraData.cameraFocus = cameraData.translation;
					result.dataChanged = true;
				}
				result.dataChanged |= ImGui::DragFloat3(
					"Camera Focus",
					&cameraData.cameraFocus.x,
					0.05f);
			}
		}

		if (objectData.objectType == "EVENT_CAMERA") {
			ImGui::SeparatorText("Event Camera");
			ImGui::TextDisabled("Position is the camera location. Camera Focus is the point it looks at.");
			if (findTriggerIndexForCamera(objectData.name) < 0) {
				ImGui::TextColored(
					ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
					"This camera has no Event Trigger, so it will not become active.");
			}
			if (!objectData.hasCameraFocus) {
				objectData.hasCameraFocus = true;
				objectData.cameraFocus = objectData.translation;
				result.dataChanged = true;
			}
			result.dataChanged |= ImGui::DragFloat3(
				"Camera Focus",
				&objectData.cameraFocus.x,
				0.05f);
		}

		if (objectData.objectType == "CAMERA_AREA") {
			ImGui::SeparatorText("Camera Area");
			ImGui::TextDisabled("This box keeps player control and changes only the follow camera settings.");
			if (!objectData.hasCameraArea) {
				objectData.hasCameraArea = true;
				result.dataChanged = true;
			}
			if (!objectData.hasCollider) {
				objectData.hasCollider = true;
				objectData.collider.type = "BOX";
				objectData.collider.size = { 8.0f, 6.0f, 8.0f };
				result.dataChanged = true;
			}
			result.dataChanged |= ImGui::DragFloat(
				"Camera Distance",
				&objectData.cameraArea.distance,
				0.05f,
				3.0f,
				20.0f);
			float pitchDegrees = objectData.cameraArea.pitch * kRadianToDegree;
			if (ImGui::DragFloat("Camera Pitch (deg)", &pitchDegrees, 0.5f, -8.0f, 65.0f)) {
				objectData.cameraArea.pitch = pitchDegrees * kDegreeToRadian;
				result.dataChanged = true;
			}
			result.dataChanged |= ImGui::DragFloat(
				"Base FOV",
				&objectData.cameraArea.fovY,
				0.005f,
				0.2f,
				1.2f);
		}

		if (objectData.objectType == "PATH_OBJECT") {
			ImGui::SeparatorText("Control Point Path");
			ImGui::TextDisabled("Path movement runs while Game View is active.");
			result.dataChanged |= ImGui::DragFloat(
				"Path Speed",
				&objectData.pathSpeed,
				0.05f,
				0.0f,
				20.0f);
			result.dataChanged |= ImGui::Checkbox(
				"Loop Path",
				&objectData.pathLoop);

			int removeControlPointIndex = -1;
			for (int pointIndex = 0;
				pointIndex < static_cast<int>(objectData.controlPoints.size());
				++pointIndex) {
				ImGui::PushID(pointIndex);
				ImGui::Text("Point %d", pointIndex);
				ImGui::SameLine();
				if (ImGui::SmallButton("Remove")) {
					removeControlPointIndex = pointIndex;
				}
				result.dataChanged |= ImGui::DragFloat3(
					"Offset",
					&objectData.controlPoints[pointIndex].x,
					0.05f);
				ImGui::PopID();
			}
			if (removeControlPointIndex >= 0 &&
				objectData.controlPoints.size() > 2) {
				objectData.controlPoints.erase(
					objectData.controlPoints.begin() + removeControlPointIndex);
				result.dataChanged = true;
			}
			if (ImGui::Button("Add Control Point")) {
				Vector3 newPoint{};
				if (!objectData.controlPoints.empty()) {
					newPoint = objectData.controlPoints.back();
					newPoint.z += 2.0f;
				}
				objectData.controlPoints.push_back(newPoint);
				result.dataChanged = true;
			}
		}

		if (ImGui::CollapsingHeader("Object Actions")) {
			ImGui::Text("Selected: %s", objectData.name.c_str());
			result.addSphereRequested = ImGui::Button("Add Sphere Object");
			ImGui::SameLine();
			result.addEventPairRequested = ImGui::Button("Add Event Camera + Trigger");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Adds a camera and its detection box. The new camera is selected for editing.");
			}
			result.addCameraAreaRequested = ImGui::Button("Add Camera Area");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Adds a box that changes distance, pitch, and FOV while player stays in control.");
			}
			result.addPathBoxRequested = ImGui::Button("Add Path Box");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Optional moving block or obstacle. Edit its path and BOX collider in Object Details.");
			}
		}
	}
	ImGui::End();
#else
	(void)autoReload;
	(void)filePath;
	(void)status;
	(void)levelData;
	(void)selectedObjectIndex;
	(void)canUndo;
	(void)canRedo;
#endif
	return result;
}
