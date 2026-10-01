#include "StageLevelEditor.h"

#include <algorithm>

// Stage1用Model Shelfのresources一覧を作ります。
void StageLevelEditor::Initialize()
{
	SceneEditor::ScanResourceShelf(shelfState_);
}

// Model Shelfからの追加・一括削除を、Stage1が渡したLevelData操作へ変換します。
void StageLevelEditor::DrawModelShelf(const Context& context)
{
#ifdef USE_IMGUI
	SceneEditor::ShelfCallbacks callbacks{};
	callbacks.sceneLabel = "Stage1 Edit View";
	if (context.levelData) {
		callbacks.addedModelCount = static_cast<size_t>(std::count_if(
			context.levelData->objects.begin(),
			context.levelData->objects.end(),
			[](const LevelLoader::ObjectData& objectData) { return objectData.tag == "EditorAdded"; }));
	}
	callbacks.addModel = [this, &context](const std::string& fileName)
	{
		const bool added = AddModel(context, fileName);
		if (added && context.saveLevelData) {
			context.saveLevelData();
		}
		return added;
	};
	callbacks.addTexture = [](const std::string&) { return false; };
	callbacks.clearAdded = [this, &context]() { ClearEditorAddedObjects(context); };
	SceneEditor::DrawModelShelf(shelfState_, callbacks);
#else
	(void)context;
#endif
}

// Edit ViewへのDrag & DropをModel Shelfと同じモデル追加処理へ渡します。
void StageLevelEditor::HandleShelfDropOnEditView(const Context& context)
{
#ifdef USE_IMGUI
	SceneEditor::ShelfCallbacks callbacks{};
	callbacks.sceneLabel = "Stage1 Edit View";
	callbacks.addModel = [this, &context](const std::string& fileName)
	{
		const bool added = AddModel(context, fileName);
		if (added && context.saveLevelData) {
			context.saveLevelData();
		}
		return added;
	};
	callbacks.addTexture = [](const std::string&) { return false; };
	SceneEditor::HandleShelfDropOnEditView(shelfState_, callbacks);
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

// Event CameraとPlayer侵入用Triggerを、同じ番号の一組として追加します。
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
	context.levelData->objects.push_back(std::move(cameraData));
	context.levelData->objects.push_back(std::move(triggerData));
	*context.selectedObjectIndex = static_cast<int>(previousObjectCount);
	if (!context.applyLevelData(true)) {
		context.levelData->objects.resize(previousObjectCount);
		if (context.setStatus) {
			context.setStatus("Add failed. Event pair could not be created.");
		}
		return false;
	}
	if (context.setStatus) {
		context.setStatus("Added Event Trigger and Event Camera.");
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

// 制御点に沿って往復するsphere.objを、Player左前方へ追加します。
bool StageLevelEditor::AddPathSphere(const Context& context)
{
	if (!context.levelData || !context.selectedObjectIndex) {
		if (context.setStatus) {
			context.setStatus("Add failed. No map data is loaded.");
		}
		return false;
	}

	LevelLoader::ObjectData objectData{};
	objectData.type = "MESH";
	objectData.name = MakeUniqueName(*context.levelData, "PathSphere");
	objectData.tag = "MapObject";
	objectData.objectType = "PATH_OBJECT";
	objectData.fileName = "debug/sphere.obj";
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
	return CommitAddedObject(context, std::move(objectData), "Added control point path sphere.", "Add failed. Path sphere could not be created.");
}

// 選択中のObjectが床・鏡以外なら削除し、実行中モデルへ反映します。
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
	context.levelData->objects.erase(context.levelData->objects.begin() + *context.selectedObjectIndex);
	*context.selectedObjectIndex = context.levelData->objects.empty()
		? -1
		: std::clamp(*context.selectedObjectIndex, 0, static_cast<int>(context.levelData->objects.size()) - 1);
	context.applyLevelData(true);
	if (context.setStatus) {
		context.setStatus("Removed selected object.");
	}
	return true;
}

// EditorAddedタグのモデルだけを消し、実行中モデルの再構築とJSON保存まで行います。
void StageLevelEditor::ClearEditorAddedObjects(const Context& context)
{
	if (!context.levelData || !context.selectedObjectIndex) {
		return;
	}
	std::vector<LevelLoader::ObjectData>& objects = context.levelData->objects;
	objects.erase(std::remove_if(objects.begin(), objects.end(), [](const LevelLoader::ObjectData& objectData)
	{
		return objectData.tag == "EditorAdded";
	}), objects.end());
	*context.selectedObjectIndex = objects.empty()
		? -1
		: std::clamp(*context.selectedObjectIndex, 0, static_cast<int>(objects.size()) - 1);
	if (context.applyLevelData) {
		context.applyLevelData(true);
	}
	if (context.saveLevelData) {
		context.saveLevelData();
	}
	if (context.setStatus) {
		context.setStatus("Cleared models added from Edit View.");
	}
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
	} while (std::any_of(levelData.objects.begin(), levelData.objects.end(), [&](const LevelLoader::ObjectData& objectData)
	{
		return objectData.name == objectName;
	}));
	return objectName;
}

// 追加したObjectDataを反映し、生成に失敗した時だけデータを元に戻します。
bool StageLevelEditor::CommitAddedObject(
	const Context& context,
	LevelLoader::ObjectData objectData,
	const std::string& successMessage,
	const std::string& failureMessage)
{
	if (!context.levelData || !context.selectedObjectIndex || !context.applyLevelData) {
		return false;
	}
	context.levelData->objects.push_back(std::move(objectData));
	*context.selectedObjectIndex = static_cast<int>(context.levelData->objects.size()) - 1;
	if (!context.applyLevelData(true)) {
		context.levelData->objects.pop_back();
		if (context.setStatus) {
			context.setStatus(failureMessage);
		}
		return false;
	}
	if (context.setStatus) {
		context.setStatus(successMessage);
	}
	return true;
}

// Scene切替時に、前回のShelf選択と表示メッセージを解放します。
void StageLevelEditor::Finalize()
{
	shelfState_.entries.clear();
	shelfState_.selectedEntry.clear();
	shelfState_.message.clear();
}
