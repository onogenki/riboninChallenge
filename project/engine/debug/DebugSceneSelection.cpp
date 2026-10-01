#include "DebugSceneSelection.h"

#include "DebugEntityRegistry.h"
#include "Object3d.h"
#include "Sprite.h"

// 指定3Dモデルを選択し、対応するECS Entityも選択します。
bool DebugSceneSelection::SelectObject(
	bool isAnimationObject,
	size_t index,
	const std::vector<std::unique_ptr<Object3d>>& normalObjects,
	const std::vector<std::unique_ptr<Object3d>>& animationObjects,
	DebugEntityRegistry& entityRegistry)
{
	const std::vector<std::unique_ptr<Object3d>>& objects =
		isAnimationObject ? animationObjects : normalObjects;
	if (index >= objects.size() || !objects[index]) {
		ClearObjectSelection();
		return false;
	}

	objectSelection_.hasSelection = true;
	objectSelection_.isAnimationObject = isAnimationObject;
	objectSelection_.index = index;
	objectInspectorAutoSelectFrames_ = 2;
	entityRegistry.SelectObject(objects[index].get());
	ClearSpriteSelection();
	return true;
}

// 指定Spriteを選択し、対応するECS Entityも選択します。
bool DebugSceneSelection::SelectSprite(
	size_t index,
	const std::vector<std::unique_ptr<Sprite>>& sprites,
	DebugEntityRegistry& entityRegistry)
{
	if (index >= sprites.size() || !sprites[index]) {
		ClearSpriteSelection();
		return false;
	}

	spriteSelection_.hasSelection = true;
	spriteSelection_.index = index;
	spriteInspectorAutoSelectFrames_ = 2;
	entityRegistry.SelectSprite(sprites[index].get());
	ClearObjectSelection();
	return true;
}

// 3Dモデルの選択状態とInspector自動選択を解除します。
void DebugSceneSelection::ClearObjectSelection()
{
	objectSelection_ = {};
	objectInspectorAutoSelectFrames_ = 0;
}

// Spriteの選択状態とInspector自動選択を解除します。
void DebugSceneSelection::ClearSpriteSelection()
{
	spriteSelection_ = {};
	spriteInspectorAutoSelectFrames_ = 0;
}

// Scene終了時に、3DモデルとSpriteの選択状態をまとめて解除します。
void DebugSceneSelection::ClearAll()
{
	ClearObjectSelection();
	ClearSpriteSelection();
}

// Inspectorで追加直後の対象を選び続ける残りフレームを一つ進めます。
void DebugSceneSelection::UpdateInspectorAutoSelection()
{
	if (objectInspectorAutoSelectFrames_ > 0) {
		--objectInspectorAutoSelectFrames_;
	}
	if (spriteInspectorAutoSelectFrames_ > 0) {
		--spriteInspectorAutoSelectFrames_;
	}
}

// 3Dモデルが選択され、Inspectorの自動選択期間内かを返します。
bool DebugSceneSelection::IsObjectInspectorAutoSelected() const
{
	return objectSelection_.hasSelection && objectInspectorAutoSelectFrames_ > 0;
}

// Spriteが選択され、Inspectorの自動選択期間内かを返します。
bool DebugSceneSelection::IsSpriteInspectorAutoSelected() const
{
	return spriteSelection_.hasSelection && spriteInspectorAutoSelectFrames_ > 0;
}
