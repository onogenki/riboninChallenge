#include "DebugEditViewport.h"

#include "ImGuiManager.h"
#include "Object3d.h"
#include "Sprite.h"

// 3DモデルとSpriteのEdit Viewを描画し、選択変更をSceneへ返します。
void DebugEditViewport::Draw(const Context& context)
{
	if (!ImGuiManager::GetInstance()->IsEditViewActive() || context.isAssetPreviewActive ||
		!context.normalObjects || !context.animationObjects || !context.sprites ||
		!context.modelViewportState || !context.spriteViewportState) {
		return;
	}

	DrawModelViewport(context);
	DrawSpriteViewport(context);
}

// 通常・Animationモデルを一つの3D Viewへ並べます。
void DebugEditViewport::DrawModelViewport(const Context& context)
{
#ifdef USE_IMGUI
	SceneEditor::ViewportOptions options{};
	options.camera = context.camera;
	for (size_t index = 0; index < context.normalObjects->size(); ++index) {
		Object3d* object = (*context.normalObjects)[index].get();
		const std::string modelName = object && !object->GetModelName().empty()
			? object->GetModelName()
			: "Model";
		options.objects.push_back({ modelName + " [" + std::to_string(index) + "]", object });
	}
	const size_t animationOffset = options.objects.size();
	for (size_t index = 0; index < context.animationObjects->size(); ++index) {
		Object3d* object = (*context.animationObjects)[index].get();
		const std::string modelName = object && !object->GetModelName().empty()
			? object->GetModelName()
			: "Animation Model";
		options.objects.push_back({ modelName + " [Animation " + std::to_string(index) + "]", object });
	}

	context.modelViewportState->selectedIndex = context.hasSelectedObject
		? static_cast<int>(context.selectedObjectIsAnimation
			? animationOffset + context.selectedObjectIndex
			: context.selectedObjectIndex)
		: -1;
	options.onSelectionChanged = [&context, animationOffset](int index) {
		if (index < 0) {
			if (context.clearObjectSelection) {
				context.clearObjectSelection();
			}
			return;
		}
		if (context.selectObject) {
			const bool isAnimation = static_cast<size_t>(index) >= animationOffset;
			const size_t objectIndex = isAnimation
				? static_cast<size_t>(index) - animationOffset
				: static_cast<size_t>(index);
			context.selectObject(isAnimation, objectIndex);
		}
	};
	SceneEditor::DrawViewportEditor(*context.modelViewportState, options);
#else
	static_cast<void>(context);
#endif
}

// Spriteを2D Viewへ並べます。
void DebugEditViewport::DrawSpriteViewport(const Context& context)
{
#ifdef USE_IMGUI
	SceneEditor::SpriteViewportOptions options{};
	for (size_t index = 0; index < context.sprites->size(); ++index) {
		options.sprites.push_back({
			"2D Texture [" + std::to_string(index) + "]",
			(*context.sprites)[index].get(),
		});
	}
	context.spriteViewportState->selectedIndex =
		context.hasSelectedSprite && context.selectedSpriteIndex < context.sprites->size()
		? static_cast<int>(context.selectedSpriteIndex)
		: -1;
	options.onSelectionChanged = [&context](int index) {
		if (index < 0) {
			if (context.clearSpriteSelection) {
				context.clearSpriteSelection();
			}
			return;
		}
		if (context.selectSprite) {
			context.selectSprite(static_cast<size_t>(index));
		}
	};
	SceneEditor::DrawSpriteViewportEditor(*context.spriteViewportState, options);
#else
	static_cast<void>(context);
#endif
}
