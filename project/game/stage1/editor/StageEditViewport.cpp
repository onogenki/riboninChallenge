#include "StageEditViewport.h"

#include "Camera.h"
#include "CarryableMirror.h"
#include "FixedMirror.h"
#include "LevelLoader.h"
#include "StageMapRuntime.h"
#include <algorithm>

// JSON由来の見た目をViewportへ並べ、選択したObjectDataへギズモ編集結果を反映します。
void StageEditViewport::Draw(const Context& context)
{
#ifdef USE_IMGUI
	if (!context.levelData || !context.fixedMirrors || !context.mapRuntime || !context.selectedObjectIndex) {
		return;
	}

	SceneEditor::ViewportOptions options{};
	options.camera = context.camera;
	std::vector<std::string> sourceNames;
	const auto addObject = [&](const std::string& sourceName, Object3d* object)
	{
		if (!object) {
			return;
		}
		sourceNames.push_back(sourceName);
		options.objects.push_back({ sourceName, object });
	};

	size_t fixedMirrorIndex = 0;
	for (const LevelLoader::ObjectData& objectData : context.levelData->objects) {
		if (objectData.tag == "Floor") {
			addObject(objectData.name, context.floor);
		} else if (objectData.objectType == "CARRYABLE_MIRROR" && context.carryableMirror) {
			// 固定鏡とは別に、JSONで置く持てる鏡も選択・移動できるようにします。
			addObject(objectData.name, &context.carryableMirror->GetObject());
		} else if (objectData.tag == "Mirror") {
			if (fixedMirrorIndex < context.fixedMirrors->size() && (*context.fixedMirrors)[fixedMirrorIndex]) {
				addObject(objectData.name, &(*context.fixedMirrors)[fixedMirrorIndex]->GetObject());
			}
			++fixedMirrorIndex;
		}
	}
	for (StageMapRuntime::RuntimeObject& runtimeObject : context.mapRuntime->GetObjects()) {
		addObject(runtimeObject.sourceName, runtimeObject.visual.get());
	}

	viewportState_.selectedIndex = -1;
	if (*context.selectedObjectIndex >= 0 &&
		*context.selectedObjectIndex < static_cast<int>(context.levelData->objects.size())) {
		const std::string& selectedName = context.levelData->objects[*context.selectedObjectIndex].name;
		const auto found = std::find(sourceNames.begin(), sourceNames.end(), selectedName);
		if (found != sourceNames.end()) {
			viewportState_.selectedIndex = static_cast<int>(std::distance(sourceNames.begin(), found));
		}
	}

	options.onSelectionChanged = [&context, &sourceNames](int viewportIndex)
	{
		if (viewportIndex < 0 || viewportIndex >= static_cast<int>(sourceNames.size())) {
			return;
		}
		const std::string& selectedName = sourceNames[viewportIndex];
		for (int objectIndex = 0; objectIndex < static_cast<int>(context.levelData->objects.size()); ++objectIndex) {
			if (context.levelData->objects[objectIndex].name == selectedName) {
				*context.selectedObjectIndex = objectIndex;
				return;
			}
		}
	};
	options.onTransformChanged = [&context, &sourceNames](int viewportIndex, const Transform& transform)
	{
		if (viewportIndex < 0 || viewportIndex >= static_cast<int>(sourceNames.size())) {
			return;
		}
		const std::string& selectedName = sourceNames[viewportIndex];
		for (LevelLoader::ObjectData& objectData : context.levelData->objects) {
			if (objectData.name != selectedName) {
				continue;
			}
			objectData.translation = transform.translate;
			objectData.rotation = transform.rotate;
			objectData.scaling = transform.scale;
			if (context.applyEdits && context.applyEdits() && context.setStatus) {
				context.setStatus("Edited in Edit View. Press Save Map to keep it.");
			}
			return;
		}
	};
	SceneEditor::DrawViewportEditor(viewportState_, options);
#else
	(void)context;
#endif
}

// Sceneを切り替えた時に前のViewport選択・Drag状態を持ち込まないよう初期化します。
void StageEditViewport::Finalize()
{
	viewportState_ = {};
}
