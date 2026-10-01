#include "DebugLevelRuntime.h"

#include "DebugEntityRegistry.h"
#include "DebugSceneSelection.h"
#include "Object3d.h"
#include "Object3dFactory.h"

#include <algorithm>

// 読むLevel名・監視ファイル・Debug初期モデル数を設定します。
void DebugLevelRuntime::Initialize(
	const std::string& levelFileName,
	const std::string& watchFilePath,
	size_t debugInitialObjectCount)
{
	levelFileName_ = levelFileName;
	hotReload_.SetFilePath(watchFilePath);
	debugInitialObjectCount_ = debugInitialObjectCount;
	loadedObjectCount_ = 0;
	isEcsSyncReady_ = false;
}

// scene.jsonを読み、Debug初期モデルとEditor追加モデルを残したままJSON由来部分だけを入れ替えます。
bool DebugLevelRuntime::Reload(const Context& context)
{
	if (!context.object3dCommon || !context.normalObjects) {
		return false;
	}

	std::unique_ptr<LevelLoader::LevelData> levelData =
		LevelLoader::Load(levelFileName_);
	if (!levelData) {
		return false;
	}

	std::vector<std::unique_ptr<Object3d>> loadedObjects;
	CreateLevelObjects(levelData->objects, context.object3dCommon, loadedObjects);

	const size_t previousLevelEnd = (std::min)(
		debugInitialObjectCount_ + loadedObjectCount_, context.normalObjects->size());
	std::vector<std::unique_ptr<Object3d>> editorAddedObjects;
	for (size_t index = previousLevelEnd; index < context.normalObjects->size(); ++index) {
		editorAddedObjects.push_back(std::move((*context.normalObjects)[index]));
	}
	context.normalObjects->resize((std::min)(debugInitialObjectCount_, context.normalObjects->size()));

	for (std::unique_ptr<Object3d>& object : loadedObjects) {
		context.normalObjects->push_back(std::move(object));
	}
	loadedObjectCount_ = context.normalObjects->size() - debugInitialObjectCount_;

	for (std::unique_ptr<Object3d>& object : editorAddedObjects) {
		context.normalObjects->push_back(std::move(object));
	}

	if (isEcsSyncReady_ && context.entityRegistry && context.animationObjects && context.sprites) {
		for (size_t index = debugInitialObjectCount_;
			index < debugInitialObjectCount_ + loadedObjectCount_; ++index) {
			const std::unique_ptr<Object3d>& object = (*context.normalObjects)[index];
			if (object) {
				context.entityRegistry->RegisterInitialModel(
					object.get(), object->GetModelName(), false);
			}
		}
		context.entityRegistry->Synchronize(
			*context.normalObjects,
			*context.animationObjects,
			*context.sprites);
		if (context.selection) {
			context.selection->ClearObjectSelection();
		}
	}

	return true;
}

// scene.jsonが保存された時だけReloadします。
void DebugLevelRuntime::UpdateHotReload(const Context& context)
{
	if (hotReload_.ConsumeChange()) {
		Reload(context);
	}
}

// 初回読込直後のファイル時刻を記録し、次フレームの誤検出を防ぎます。
void DebugLevelRuntime::SynchronizeWatch()
{
	hotReload_.Synchronize();
}

// Scene終了時に、次回のDebug表示へ読込状態を持ち込まないよう初期化します。
void DebugLevelRuntime::Reset()
{
	levelFileName_.clear();
	hotReload_ = {};
	debugInitialObjectCount_ = 0;
	loadedObjectCount_ = 0;
	isEcsSyncReady_ = false;
}

// JSONツリーを再帰的に調べ、MESHだけを実行中Object3dとして生成します。
void DebugLevelRuntime::CreateLevelObjects(
	const std::vector<LevelLoader::ObjectData>& objectDataList,
	Object3dCommon* object3dCommon,
	std::vector<std::unique_ptr<Object3d>>& outObjects) const
{
	for (const LevelLoader::ObjectData& objectData : objectDataList) {
		if (objectData.type == "MESH" && !objectData.fileName.empty()) {
			auto object = Object3dFactory::Create(object3dCommon, objectData.fileName);
			if (object) {
				object->SetTranslate(objectData.translation);
				object->SetRotate(objectData.rotation);
				object->SetScale(objectData.scaling);
				outObjects.push_back(std::move(object));
			}
		}

		CreateLevelObjects(objectData.children, object3dCommon, outObjects);
	}
}
