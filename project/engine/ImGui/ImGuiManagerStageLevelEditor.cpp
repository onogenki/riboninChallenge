#include "ImGuiManager.h"

#include <algorithm>
#include <numbers>

// レベルファイルの再読込と、Stage1内オブジェクトを編集するInspector UIを表示します。
LevelEditorResult ImGuiManager::LevelHotReloadWindow(
	bool& autoReload,
	const std::string& filePath,
	const std::string& status,
	LevelLoader::LevelData* levelData,
	int& selectedObjectIndex)
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

	ImGui::Separator();
	ImGui::TextUnformatted("Stage Map Editor / Hot Reload");
	ImGui::Separator();
	ImGui::TextWrapped("Edit an object here, then press Save Map. Editing the JSON file outside the game also reloads it.");
	ImGui::TextWrapped("File: %s", filePath.c_str());
	ImGui::Checkbox("Auto Reload", &autoReload);
	ImGui::TextDisabled("Auto Reload: use after saving stage1.json in an external editor.");
	result.reloadRequested = ImGui::Button("Reload Now");
	ImGui::SameLine();
	result.saveRequested = ImGui::Button("Save Map");
	ImGui::TextDisabled("Reload Now discards unsaved editor changes and reads the JSON file again.");
	ImGui::Separator();
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
		ImGui::Text("Model: %s", objectData.fileName.empty() ? "(none)" : objectData.fileName.c_str());
		ImGui::Text("Tag: %s", objectData.tag.empty() ? "(none)" : objectData.tag.c_str());

		const char* positionLabel = objectData.objectType == "EVENT_TRIGGER"
			? "Trigger Position"
			: (objectData.objectType == "CAMERA_AREA" ? "Area Position" : "Position");
		result.dataChanged |= ImGui::DragFloat3(
			positionLabel,
			&objectData.translation.x,
			0.05f);

		constexpr float kRadianToDegree = 180.0f / std::numbers::pi_v<float>;
		constexpr float kDegreeToRadian = std::numbers::pi_v<float> / 180.0f;
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

		if (objectData.hasCollider) {
			ImGui::SeparatorText("Box Collider");
			ImGui::TextDisabled("The box follows this object's Position, Rotation, and Scale.");
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

		ImGui::Separator();
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
		result.addPathSphereRequested = ImGui::Button("Add Path Sphere");
		const bool protectedObject =
			objectData.tag == "Floor" || objectData.tag == "Mirror";
		ImGui::BeginDisabled(protectedObject);
		result.removeSelectedRequested = ImGui::Button("Remove Selected");
		ImGui::EndDisabled();
		if (protectedObject && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("Floor and Mirror cannot be removed.");
		}
	}
	ImGui::End();
#else
	(void)autoReload;
	(void)filePath;
	(void)status;
	(void)levelData;
	(void)selectedObjectIndex;
#endif
	return result;
}
