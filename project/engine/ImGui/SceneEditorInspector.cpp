#include "SceneEditor.h"

#include "ImGuiManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#endif

// 選択中のSprite・モデル・Lightを、Edit View用のInspectorで編集します。
void SceneEditor::DrawInspector(const InspectorOptions& options)
{
#ifdef USE_IMGUI
	// Transformを変更できるInspectorはEdit Viewだけで表示する。
	if (!ImGuiManager::GetInstance()->IsEditViewActive()) {
		return;
	}

	if (!options.normalObjects || !options.animationObjects || !options.directionalLight || !options.pointLight || !options.spotLight) {
		return;
	}

	const unsigned int inspectorDockId = ImGuiManager::GetInstance()->GetInspectorDockId();
	if (inspectorDockId != 0) {
		ImGui::SetNextWindowDockID(inspectorDockId, options.forceDock ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
	}

	ImGui::Begin("Inspector");
	if (options.description && options.description[0] != '\0') {
		ImGui::TextWrapped("%s", options.description);
		ImGui::Separator();
	}
	if (options.drawHeader) {
		options.drawHeader();
	}

	const size_t addedNormalCount = options.normalObjects->size() > options.protectedNormalObjectCount
		? options.normalObjects->size() - options.protectedNormalObjectCount
		: 0;
	const size_t addedAnimationCount = options.animationObjects->size() > options.protectedAnimationObjectCount
		? options.animationObjects->size() - options.protectedAnimationObjectCount
		: 0;
	if (addedNormalCount > 0 || addedAnimationCount > 0 || options.addedSpriteCount > 0) {
		ImGui::TextDisabled("Added Models: %zu | 2D Textures: %zu", addedNormalCount + addedAnimationCount, options.addedSpriteCount);
	}

	if (ImGui::BeginTabBar("SceneEditorInspectorTabs")) {
		const ImGuiTabItemFlags spriteTabFlags = options.selectSpriteTab ? ImGuiTabItemFlags_SetSelected : 0;
		if (options.sprites && ImGui::BeginTabItem("Sprite", nullptr, spriteTabFlags)) {
			const int selectedSpriteIndex = ImGuiManager::GetInstance()->SpriteWindow(*options.sprites, true, options.forcedSpriteIndex);
			const bool canRemoveSprite = options.removeSprite && selectedSpriteIndex >= 0 &&
				static_cast<size_t>(selectedSpriteIndex) >= options.protectedSpriteCount;
			ImGui::Separator();
			ImGui::BeginDisabled(!canRemoveSprite);
			const bool removeSprite = ImGui::Button("Remove Added 2D Texture") ||
				(canRemoveSprite && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
					!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false));
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip(canRemoveSprite
					? "Remove this 2D Texture. Delete key also works while Inspector is focused."
					: "Initial scene sprites are protected. Only textures added from Model Shelf can be removed here.");
			}
			if (removeSprite && canRemoveSprite) {
				options.removeSprite(static_cast<size_t>(selectedSpriteIndex));
			}
			ImGui::EndTabItem();
		}
		const ImGuiTabItemFlags modelTabFlags = options.selectModelTab ? ImGuiTabItemFlags_SetSelected : 0;
		if (ImGui::BeginTabItem("Model", nullptr, modelTabFlags)) {
			ImGuiManager::GetInstance()->ModelWindow(
				*options.normalObjects,
				*options.animationObjects,
				*options.directionalLight,
				*options.pointLight,
				*options.spotLight,
				true,
				options.protectedNormalObjectCount,
				options.protectedAnimationObjectCount,
				options.forcedNormalIndex,
				options.forcedAnimationIndex,
				options.onModelRemoved);
			ImGui::EndTabItem();
		}
		if (options.drawExtraTabs) {
			options.drawExtraTabs();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
#else
	(void)options;
#endif
}
