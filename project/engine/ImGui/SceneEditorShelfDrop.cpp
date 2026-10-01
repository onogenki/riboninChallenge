#include "SceneEditor.h"

#include "ImGuiManager.h"
#include "SceneEditorShelfMessages.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#endif

// Model ShelfからGame ViewへDropされたモデル・Textureを、Sceneの追加処理へ渡します。
void SceneEditor::HandleShelfDropOnEditView(ShelfState& state, const ShelfCallbacks& callbacks)
{
#ifdef USE_IMGUI
	// Game Viewへの誤配置を防ぎ、Edit Viewだけをドロップ先として扱う。
	if (!ImGuiManager::GetInstance()->IsEditViewActive()) {
		return;
	}

	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(x, y, width, height)) {
		return;
	}

	const ImRect gameViewRect(ImVec2(x, y), ImVec2(x + width, y + height));
	const std::string dropTargetId = callbacks.sceneLabel + "EditViewShelfDropTarget";
	if (ImGui::BeginDragDropTargetCustom(gameViewRect, ImGui::GetID(dropTargetId.c_str()))) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_FILE")) {
			std::string fileName(static_cast<const char*>(payload->Data), payload->DataSize);
			if (!fileName.empty() && fileName.back() == '\0') {
				fileName.pop_back();
			}
			const ImVec2 mousePosition = ImGui::GetMousePos();
			const bool success = callbacks.addModelAtDropPosition
				? callbacks.addModelAtDropPosition(fileName, mousePosition.x, mousePosition.y)
				: callbacks.addModel && callbacks.addModel(fileName);
			state.message = SceneEditorShelfMessages::MakeAddMessage(callbacks.sceneLabel, "model", fileName, success);
			if (callbacks.afterAdd) {
				callbacks.afterAdd();
			}
		}
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_FILE")) {
			std::string textureFilePath(static_cast<const char*>(payload->Data), payload->DataSize);
			if (!textureFilePath.empty() && textureFilePath.back() == '\0') {
				textureFilePath.pop_back();
			}
			const ImVec2 mousePosition = ImGui::GetMousePos();
			const bool success = callbacks.addTextureAtDropPosition
				? callbacks.addTextureAtDropPosition(textureFilePath, mousePosition.x, mousePosition.y)
				: callbacks.addTexture && callbacks.addTexture(textureFilePath);
			state.message = SceneEditorShelfMessages::MakeAddMessage(callbacks.sceneLabel, "2D Texture", textureFilePath, success);
			if (callbacks.afterAdd) {
				callbacks.afterAdd();
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::IsDragDropActive() && gameViewRect.Contains(ImGui::GetMousePos())) {
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->AddRect(gameViewRect.Min, gameViewRect.Max, IM_COL32(80, 180, 255, 255), 0.0f, 0, 4.0f);
		const std::string dropLabel = "Drop into " + callbacks.sceneLabel;
		drawList->AddText(ImVec2(gameViewRect.Min.x + 16.0f, gameViewRect.Min.y + 16.0f), IM_COL32(180, 230, 255, 255), dropLabel.c_str());
	}
#else
	(void)state;
	(void)callbacks;
#endif
}
