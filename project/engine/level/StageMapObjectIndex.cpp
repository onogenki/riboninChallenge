#include "StageMapObjectIndex.h"

namespace
{
// 読込時の分類と同じ順番で調べ、最後のPlayerStartを編集対象にします。
LevelLoader::ObjectData* FindPlayerStartInObjects(std::vector<LevelLoader::ObjectData>& objects)
{
	LevelLoader::ObjectData* found = nullptr;
	for (LevelLoader::ObjectData& objectData : objects) {
		if (objectData.tag == "PlayerStart") {
			found = &objectData;
		}
		if (LevelLoader::ObjectData* child = FindPlayerStartInObjects(objectData.children)) {
			found = child;
		}
	}
	return found;
}

// 読込時と同じ深さ優先順で探し、同じ種類が複数ある場合は最後のデータを返します。
LevelLoader::ObjectData* FindObjectTypeInObjects(std::vector<LevelLoader::ObjectData>& objects, const std::string& objectType)
{
	LevelLoader::ObjectData* found = nullptr;
	for (LevelLoader::ObjectData& objectData : objects) {
		if (objectData.objectType == objectType) {
			found = &objectData;
		}
		if (LevelLoader::ObjectData* child = FindObjectTypeInObjects(objectData.children, objectType)) {
			found = child;
		}
	}
	return found;
}
}

// JSONへ保存できる元データを返し、実行中Playerの位置とは分けて編集します。
LevelLoader::ObjectData* StageMapObjectIndex::FindPlayerStartForEdit(LevelLoader::LevelData& levelData)
{
	return FindPlayerStartInObjects(levelData.objects);
}

LevelLoader::ObjectData* StageMapObjectIndex::FindObjectTypeForEdit(LevelLoader::LevelData& levelData, const std::string& objectType)
{
	return FindObjectTypeInObjects(levelData.objects, objectType);
}

// 鏡の追加や順番変更で操作対象が変わらないよう、JSON内で明示された一枚を選びます。
std::size_t StageMapObjectIndex::GetPuzzleMirrorIndex() const
{
	for (std::size_t index = 0; index < fixedMirrors_.size(); ++index) {
		if (fixedMirrors_[index]->objectType == "PUZZLE_MIRROR") {
			return index;
		}
	}
	return 0;
}

// 以前のLevelDataを参照しないよう、全一覧を空にしてから現在のLevelDataを分類します。
void StageMapObjectIndex::Build(const LevelLoader::LevelData& levelData)
{
	floor_ = nullptr;
	playerStart_ = nullptr;
	carryableMirror_ = nullptr;
	chargeSwitch_ = nullptr;
	mirrorFloor_ = nullptr;
	orbitLight_ = nullptr;
	staticLights_.clear();
	fixedMirrors_.clear();
	runtimeObjects_.clear();
	eventTriggers_.clear();
	eventCameras_.clear();
	cameraAreas_.clear();
	Collect(levelData.objects);
}

// タグ・object_typeごとに使用者を決め、どのStageでも同じ分類規則を使えるようにします。
void StageMapObjectIndex::Collect(const std::vector<LevelLoader::ObjectData>& objects)
{
	for (const LevelLoader::ObjectData& objectData : objects) {
		if (objectData.tag == "Floor") {
			floor_ = &objectData;
		} else if (objectData.tag == "PlayerStart") {
			playerStart_ = &objectData;
		} else if (objectData.objectType == "CARRYABLE_MIRROR") {
			carryableMirror_ = &objectData;
		} else if (objectData.objectType == "CHARGE_SWITCH") {
			chargeSwitch_ = &objectData;
		} else if (objectData.objectType == "MIRROR_FLOOR") {
			mirrorFloor_ = &objectData;
		} else if (objectData.objectType == "ORBIT_LIGHT") {
			orbitLight_ = &objectData;
		} else if (objectData.objectType == "STATIC_LIGHT") {
			staticLights_.push_back(&objectData);
		} else if (objectData.tag == "Mirror") {
			fixedMirrors_.push_back(&objectData);
		} else if (objectData.objectType == "EVENT_TRIGGER") {
			eventTriggers_.push_back(&objectData);
		} else if (objectData.objectType == "EVENT_CAMERA") {
			eventCameras_.push_back(&objectData);
		} else if (objectData.objectType == "CAMERA_AREA" && objectData.hasCameraArea) {
			cameraAreas_.push_back(&objectData);
		} else if (objectData.type == "MESH" && !objectData.fileName.empty()) {
			runtimeObjects_.push_back(&objectData);
		}
		Collect(objectData.children);
	}
}
