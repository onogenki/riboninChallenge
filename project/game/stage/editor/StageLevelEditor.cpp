#include "StageLevelEditor.h"

#include "Model.h"
#include "StageMapRuntime.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
	// 追加名・改名の両方で、子Objectを含む同名を一つの規則で調べます。
	bool HasObjectName(
		const std::vector<LevelLoader::ObjectData>& objects,
		const std::string& name,
		const LevelLoader::ObjectData* excluded = nullptr)
	{
		for (const LevelLoader::ObjectData& objectData : objects) {
			if (&objectData != excluded && objectData.name == name) {
				return true;
			}
			if (HasObjectName(objectData.children, name, excluded)) {
				return true;
			}
		}
		return false;
	}
}

// 各Stageで共通に使うModel Shelfのresources一覧を作ります。
void StageLevelEditor::Initialize()
{
	SceneEditor::ScanResourceShelf(shelfState_);
}

// Model Shelfからの追加・一括削除を現在Stageのメモリへ反映し、明示保存までJSONを変更しません。
void StageLevelEditor::DrawModelShelf(const Context& context)
{
#ifdef USE_IMGUI
	SceneEditor::ShelfCallbacks callbacks{};
	std::string actionMessage;
	callbacks.sceneLabel = "Stage " + std::to_string(context.stageId) + " Edit View";
	if (context.levelData) {
		callbacks.addedModelCount = static_cast<size_t>(std::count_if(
			context.levelData->objects.begin(),
			context.levelData->objects.end(),
			[](const LevelLoader::ObjectData& objectData) { return objectData.tag == "EditorAdded"; }));
	}
	callbacks.addModel = [this, &context, &actionMessage](const std::string& fileName)
	{
		const bool added = AddModel(context, fileName);
		if (added) {
			actionMessage = "Model added in memory. Press Save Map to keep it.";
		}
		return added;
	};
	callbacks.addTexture = [](const std::string&) { return false; };
	callbacks.clearAdded = [this, &context, &actionMessage]()
	{
		actionMessage = ClearEditorAddedObjects(context);
	};
	SceneEditor::DrawModelShelf(shelfState_, callbacks);
	if (!actionMessage.empty()) {
		shelfState_.message = actionMessage;
	}
#else
	(void)context;
#endif
}

// Edit ViewへのDrag & Dropもメモリだけに追加し、Save Mapを押すまでJSONを変更しません。
void StageLevelEditor::HandleShelfDropOnEditView(const Context& context)
{
#ifdef USE_IMGUI
	SceneEditor::ShelfCallbacks callbacks{};
	std::string actionMessage;
	callbacks.sceneLabel = "Stage " + std::to_string(context.stageId) + " Edit View";
	callbacks.addModel = [this, &context, &actionMessage](const std::string& fileName)
	{
		const bool added = AddModel(context, fileName);
		if (added) {
			actionMessage = "Model added in memory. Press Save Map to keep it.";
		}
		return added;
	};
	callbacks.addTexture = [](const std::string&) { return false; };
	SceneEditor::HandleShelfDropOnEditView(shelfState_, callbacks);
	if (!actionMessage.empty()) {
		shelfState_.message = actionMessage;
	}
#else
	(void)context;
#endif
}

// 指定モデルをPlayer前方へ追加し、実行中モデルを再構築します。
bool StageLevelEditor::AddModel(const Context& context, const std::string& fileName)
{
	if (!context.levelData || !context.selectedObjectIndex || fileName.empty()) {
		if (context.setStatus) {
			context.setStatus("Add failed. No map data or model name.");
		}
		return false;
	}

	LevelLoader::ObjectData objectData{};
	objectData.type = "MESH";
	objectData.name = MakeUniqueName(*context.levelData, "EditorModel");
	objectData.tag = "EditorAdded";
	objectData.objectType = "STATIC";
	objectData.fileName = fileName;
	objectData.translation = context.playerPosition;
	objectData.translation.y += 1.0f;
	objectData.translation.z += 3.0f;
	objectData.scaling = { 1.0f, 1.0f, 1.0f };
	return CommitAddedObject(
		context,
		std::move(objectData),
		"Added model from Edit View: " + fileName,
		"Add failed. Model could not be loaded: " + fileName);
}

// Editor追加モデルの名前をJSONとRuntimeで同時に変え、子Objectまで含めた同名を拒みます。
bool StageLevelEditor::RenameSelectedObject(const Context& context, const std::string& requestedName)
{
	if (!context.levelData || !context.selectedObjectIndex || !context.applyLevelData ||
		*context.selectedObjectIndex < 0 ||
		*context.selectedObjectIndex >= static_cast<int>(context.levelData->objects.size())) {
		return false;
	}
	LevelLoader::ObjectData& selected = context.levelData->objects[*context.selectedObjectIndex];
	if (selected.tag != "EditorAdded" ||
		requestedName.find_first_not_of(" \t\r\n") == std::string::npos ||
		HasObjectName(context.levelData->objects, requestedName, &selected)) {
		if (context.setStatus) {
			context.setStatus("Rename failed. Enter a unique, nonempty name for an added model.");
		}
		return false;
	}
	if (selected.name == requestedName) {
		return true;
	}
	const std::string previousName = selected.name;
	selected.name = requestedName;
	if (!context.applyLevelData(true)) {
		selected.name = previousName;
		if (context.setStatus) {
			context.setStatus("Rename failed. Current stage was kept.");
		}
		return false;
	}
	if (context.setStatus) {
		context.setStatus("Renamed " + previousName + " to " + selected.name + ". Press Save Map to keep it.");
	}
	return true;
}

// Runtimeが所有する通常モデルだけにBOX判定を作り、そのモデル名を結果へ明示します。
bool StageLevelEditor::AddBoxCollider(const Context& context)
{
	if (!context.levelData || !context.selectedObjectIndex || !context.applyLevelData ||
		*context.selectedObjectIndex < 0 ||
		*context.selectedObjectIndex >= static_cast<int>(context.levelData->objects.size())) {
		return false;
	}
	LevelLoader::ObjectData& selected = context.levelData->objects[*context.selectedObjectIndex];
	const bool isRuntimeMesh = selected.type == "MESH" && !selected.fileName.empty() &&
		selected.tag != "Mirror" && selected.tag != "Floor" && selected.tag != "PlayerStart" &&
		selected.objectType != "CARRYABLE_MIRROR" && selected.objectType != "EVENT_TRIGGER" &&
		selected.objectType != "EVENT_CAMERA" && selected.objectType != "CAMERA_AREA";
	if (!isRuntimeMesh || selected.hasCollider) {
		if (context.setStatus) {
			context.setStatus("Collider was not added. Select a normal model without a collider.");
		}
		return false;
	}
	const LevelLoader::ColliderData previousCollider = selected.collider;
	selected.hasCollider = true;
	selected.collider.type = "BOX";
	selected.collider.center = {};
	selected.collider.size = { 1.0f, 1.0f, 1.0f };
	bool fittedToModel = false;
	if (context.mapRuntime) {
		const auto& runtimeObjects = context.mapRuntime->GetObjects();
		const auto found = std::find_if(runtimeObjects.begin(), runtimeObjects.end(),
			[&](const StageMapRuntime::RuntimeObject& runtimeObject)
			{
				return runtimeObject.sourceName == selected.name;
			});
		if (found != runtimeObjects.end() && found->visual && found->visual->GetModel()) {
			const auto& vertices = found->visual->GetModel()->GetModelData().vertices;
			Vector3 minPoint{
				(std::numeric_limits<float>::max)(),
				(std::numeric_limits<float>::max)(),
				(std::numeric_limits<float>::max)(),
			};
			Vector3 maxPoint{
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
			};
			for (const Engine::Model::VertexData& vertex : vertices) {
				if (!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y) ||
					!std::isfinite(vertex.position.z)) {
					continue;
				}
				minPoint.x = (std::min)(minPoint.x, vertex.position.x);
				minPoint.y = (std::min)(minPoint.y, vertex.position.y);
				minPoint.z = (std::min)(minPoint.z, vertex.position.z);
				maxPoint.x = (std::max)(maxPoint.x, vertex.position.x);
				maxPoint.y = (std::max)(maxPoint.y, vertex.position.y);
				maxPoint.z = (std::max)(maxPoint.z, vertex.position.z);
				fittedToModel = true;
			}
			if (fittedToModel) {
				selected.collider.center = {
					(minPoint.x + maxPoint.x) * 0.5f,
					(minPoint.y + maxPoint.y) * 0.5f,
					(minPoint.z + maxPoint.z) * 0.5f,
				};
				selected.collider.size = {
					(std::max)(maxPoint.x - minPoint.x, 0.1f),
					(std::max)(maxPoint.y - minPoint.y, 0.1f),
					(std::max)(maxPoint.z - minPoint.z, 0.1f),
				};
			}
		}
	}
	if (!context.applyLevelData(false)) {
		selected.hasCollider = false;
		selected.collider = previousCollider;
		if (context.setStatus) {
			context.setStatus("Collider could not be applied. Current model was kept.");
		}
		return false;
	}
	if (context.setStatus) {
		context.setStatus("Box Collider added to " + selected.name +
			(fittedToModel ? ". Fitted to model; review size, then press Save Map."
				: ". Default size 1; adjust size, then press Save Map."));
	}
	return true;
}

// Box Collider付きのsphere.objを、Player前方へ追加します。
bool StageLevelEditor::AddSphere(const Context& context)
{
	if (!context.levelData || !context.selectedObjectIndex) {
		if (context.setStatus) {
			context.setStatus("Add failed. No map data is loaded.");
		}
		return false;
	}

	LevelLoader::ObjectData objectData{};
	objectData.type = "MESH";
	objectData.name = MakeUniqueName(*context.levelData, "MapSphere");
	objectData.tag = "MapObject";
	objectData.objectType = "STATIC";
	objectData.fileName = "debug/sphere.obj";
	objectData.translation = context.playerPosition;
	objectData.translation.y += 1.5f;
	objectData.translation.z += 3.0f;
	objectData.scaling = { 1.0f, 1.0f, 1.0f };
	objectData.hasCollider = true;
	objectData.collider.type = "BOX";
	objectData.collider.size = { 1.0f, 1.0f, 1.0f };
	return CommitAddedObject(context, std::move(objectData), "Added sphere object.", "Add failed. sphere.obj could not be created.");
}

// Event CameraとPlayer侵入用Triggerを一組で追加し、失敗時は選択も元へ戻します。
bool StageLevelEditor::AddEventPair(const Context& context)
{
	if (!context.levelData || !context.selectedObjectIndex || !context.applyLevelData) {
		if (context.setStatus) {
			context.setStatus("Add failed. No map data is loaded.");
		}
		return false;
	}

	int number = 1;
	std::string cameraName;
	std::string triggerName;
	do {
		cameraName = "EventCamera" + std::to_string(number);
		triggerName = "EventTrigger" + std::to_string(number++);
	} while (std::any_of(
		context.levelData->objects.begin(),
		context.levelData->objects.end(),
		[&](const LevelLoader::ObjectData& objectData)
		{
			return objectData.name == cameraName || objectData.name == triggerName;
		}));

	const Vector3 triggerPosition{ context.playerPosition.x, context.playerPosition.y, context.playerPosition.z + 6.0f };
	LevelLoader::ObjectData cameraData{};
	cameraData.type = "CAMERA";
	cameraData.name = cameraName;
	cameraData.tag = "EventCamera";
	cameraData.objectType = "EVENT_CAMERA";
	cameraData.translation = { triggerPosition.x + 6.0f, triggerPosition.y + 4.0f, triggerPosition.z - 8.0f };
	cameraData.scaling = { 1.0f, 1.0f, 1.0f };
	cameraData.hasCameraFocus = true;
	cameraData.cameraFocus = { triggerPosition.x, triggerPosition.y + 1.0f, triggerPosition.z };

	LevelLoader::ObjectData triggerData{};
	triggerData.type = "EMPTY";
	triggerData.name = triggerName;
	triggerData.tag = "EventTrigger";
	triggerData.objectType = "EVENT_TRIGGER";
	triggerData.eventId = "Event" + std::to_string(number - 1);
	triggerData.eventCameraName = cameraName;
	triggerData.translation = triggerPosition;
	triggerData.scaling = { 1.0f, 1.0f, 1.0f };
	triggerData.hasCollider = true;
	triggerData.collider.type = "BOX";
	triggerData.collider.size = { 4.0f, 3.0f, 4.0f };

	const size_t previousObjectCount = context.levelData->objects.size();
	const int previousSelectedIndex = *context.selectedObjectIndex;
	context.levelData->objects.push_back(std::move(cameraData));
	context.levelData->objects.push_back(std::move(triggerData));
	*context.selectedObjectIndex = static_cast<int>(previousObjectCount);
	if (!context.applyLevelData(true)) {
		context.levelData->objects.resize(previousObjectCount);
		*context.selectedObjectIndex = previousSelectedIndex;
		if (context.setStatus) {
			context.setStatus("Add failed. Event pair could not be created.");
		}
		return false;
	}
	if (context.setStatus) {
		context.setStatus("Added Event Trigger and Event Camera in memory. Press Save Map to keep them.");
	}
	return true;
}

// 通常Cameraの距離・角度・視野角を変えるAreaをPlayer位置へ追加します。
bool StageLevelEditor::AddCameraArea(const Context& context)
{
	if (!context.levelData || !context.selectedObjectIndex) {
		if (context.setStatus) {
			context.setStatus("Add failed. No map data is loaded.");
		}
		return false;
	}

	LevelLoader::ObjectData areaData{};
	areaData.type = "EMPTY";
	areaData.name = MakeUniqueName(*context.levelData, "CameraArea");
	areaData.tag = "CameraArea";
	areaData.objectType = "CAMERA_AREA";
	areaData.translation = context.playerPosition;
	areaData.scaling = { 1.0f, 1.0f, 1.0f };
	areaData.hasCollider = true;
	areaData.collider.type = "BOX";
	areaData.collider.size = { 8.0f, 6.0f, 8.0f };
	areaData.hasCameraArea = true;
	areaData.cameraArea.distance = 11.5f;
	areaData.cameraArea.pitch = 0.58f;
	areaData.cameraArea.fovY = 0.48f;
	return CommitAddedObject(context, std::move(areaData), "Added Camera Area. Adjust it in the Inspector.", "Add failed. Camera Area could not be created.");
}

// BOX判定と同じ形のbox.objを、制御点で動く任意の配置物としてPlayer左前方へ追加します。
bool StageLevelEditor::AddPathBox(const Context& context)
{
	if (!context.levelData || !context.selectedObjectIndex) {
		if (context.setStatus) {
			context.setStatus("Add failed. No map data is loaded.");
		}
		return false;
	}

	LevelLoader::ObjectData objectData{};
	objectData.type = "MESH";
	objectData.name = MakeUniqueName(*context.levelData, "PathBox");
	objectData.tag = "MapObject";
	objectData.objectType = "PATH_OBJECT";
	objectData.fileName = "debug/box.obj";
	objectData.translation = context.playerPosition;
	objectData.translation.x -= 4.0f;
	objectData.translation.y += 1.5f;
	objectData.scaling = { 1.0f, 1.0f, 1.0f };
	objectData.controlPoints = {{ 0.0f, 0.0f, 0.0f }, { 2.0f, 1.0f, 2.0f }, { -2.0f, 2.0f, 4.0f }, { 0.0f, 0.0f, 6.0f }};
	objectData.pathSpeed = 1.0f;
	objectData.pathLoop = true;
	objectData.hasCollider = true;
	objectData.collider.type = "BOX";
	objectData.collider.size = { 1.0f, 1.0f, 1.0f };
	return CommitAddedObject(context, std::move(objectData), "Added control point path box.", "Add failed. Path box could not be created.");
}

// 選択中のObjectを実行中モデルへ反映してから確定し、失敗時はデータと選択を戻します。
bool StageLevelEditor::RemoveSelectedObject(const Context& context)
{
	if (!context.levelData || !context.selectedObjectIndex || !context.applyLevelData ||
		*context.selectedObjectIndex < 0 ||
		*context.selectedObjectIndex >= static_cast<int>(context.levelData->objects.size())) {
		return false;
	}

	const LevelLoader::ObjectData& selectedObject = context.levelData->objects[*context.selectedObjectIndex];
	if (selectedObject.tag == "Floor" || selectedObject.tag == "Mirror") {
		return false;
	}
	const int previousSelectedIndex = *context.selectedObjectIndex;
	LevelLoader::ObjectData removedObject = std::move(context.levelData->objects[previousSelectedIndex]);
	context.levelData->objects.erase(context.levelData->objects.begin() + previousSelectedIndex);
	*context.selectedObjectIndex = context.levelData->objects.empty()
		? -1
		: std::clamp(*context.selectedObjectIndex, 0, static_cast<int>(context.levelData->objects.size()) - 1);
	if (!context.applyLevelData(true)) {
		context.levelData->objects.insert(
			context.levelData->objects.begin() + previousSelectedIndex,
			std::move(removedObject));
		*context.selectedObjectIndex = previousSelectedIndex;
		if (context.setStatus) {
			context.setStatus("Remove failed. Current stage was kept.");
		}
		return false;
	}
	if (context.setStatus) {
		context.setStatus("Removed selected object in memory. Press Save Map to keep the removal.");
	}
	return true;
}

// EditorAddedタグだけをメモリから消し、再構築失敗時は復元して結果を棚とInspectorへ返します。
std::string StageLevelEditor::ClearEditorAddedObjects(const Context& context)
{
	if (!context.levelData || !context.selectedObjectIndex || !context.applyLevelData) {
		return "Clear failed. No map data is loaded.";
	}
	std::vector<LevelLoader::ObjectData>& objects = context.levelData->objects;
	std::vector<LevelLoader::ObjectData> previousObjects = objects;
	const int previousSelectedIndex = *context.selectedObjectIndex;
	objects.erase(std::remove_if(objects.begin(), objects.end(), [](const LevelLoader::ObjectData& objectData)
	{
		return objectData.tag == "EditorAdded";
	}), objects.end());
	*context.selectedObjectIndex = objects.empty()
		? -1
		: std::clamp(*context.selectedObjectIndex, 0, static_cast<int>(objects.size()) - 1);
	if (!context.applyLevelData(true)) {
		objects = std::move(previousObjects);
		*context.selectedObjectIndex = previousSelectedIndex;
		if (context.setStatus) {
			context.setStatus("Clear failed. Current stage was kept.");
		}
		return "Clear failed. Current stage was kept.";
	}
	const std::string message = "Models cleared in memory. Press Save Map to keep it.";
	if (context.setStatus) {
		context.setStatus(message);
	}
	return message;
}

// 同じprefixの連番を調べ、まだ使われていない最初のObject名を返します。
std::string StageLevelEditor::MakeUniqueName(
	const LevelLoader::LevelData& levelData,
	const std::string& prefix)
{
	int number = 1;
	std::string objectName;
	do {
		objectName = prefix + std::to_string(number++);
	} while (HasObjectName(levelData.objects, objectName));
	return objectName;
}

// 追加したObjectDataを反映し、生成に失敗したらデータと選択を元に戻します。
bool StageLevelEditor::CommitAddedObject(
	const Context& context,
	LevelLoader::ObjectData objectData,
	const std::string& successMessage,
	const std::string& failureMessage)
{
	if (!context.levelData || !context.selectedObjectIndex || !context.applyLevelData) {
		return false;
	}
	const int previousSelectedIndex = *context.selectedObjectIndex;
	context.levelData->objects.push_back(std::move(objectData));
	*context.selectedObjectIndex = static_cast<int>(context.levelData->objects.size()) - 1;
	if (!context.applyLevelData(true)) {
		context.levelData->objects.pop_back();
		*context.selectedObjectIndex = previousSelectedIndex;
		if (context.setStatus) {
			context.setStatus(failureMessage);
		}
		return false;
	}
	if (context.setStatus) {
		context.setStatus(successMessage + " Press Save Map to keep it.");
	}
	return true;
}

// Scene切替時に、前回のShelf選択と表示メッセージを解放します。
void StageLevelEditor::Finalize()
{
	shelfState_.entries.clear();
	ResetForStage();
}

// 次のStageへ同じShelf資源を使い回し、前Stageの選択と操作結果だけを消します。
void StageLevelEditor::ResetForStage()
{
	shelfState_.selectedEntry.clear();
	shelfState_.message.clear();
}
