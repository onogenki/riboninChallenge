#include "DebugEditOverlay.h"

#include "ImGuiManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui_internal.h"
#endif

// Edit Viewの上へ、PreviewまたはDrag & Dropの案内を描画します。
void DebugEditOverlay::Draw(const Context& context)
{
#ifdef USE_IMGUI
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

	if (!context.isPreviewActive && !ImGui::IsDragDropActive()) {
		return;
	}

	const ImVec2 gameViewMin(x, y);
	const ImVec2 gameViewMax(x + width, y + height);
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->PushClipRect(gameViewMin, gameViewMax, true);

	const ImVec2 panelMin(gameViewMin.x + 14.0f, gameViewMin.y + 14.0f);
	const ImVec2 panelMax(panelMin.x + 390.0f, panelMin.y + (context.isPreviewActive ? 88.0f : 54.0f));
	drawList->AddRectFilled(panelMin, panelMax, IM_COL32(8, 12, 18, 205), 8.0f);
	drawList->AddRect(panelMin, panelMax, IM_COL32(95, 170, 255, 210), 8.0f, 0, 1.5f);

	ImVec2 textPos(panelMin.x + 12.0f, panelMin.y + 10.0f);
	if (context.isPreviewActive) {
		const std::string title = "Preview: " + context.previewDisplayName;
		drawList->AddText(textPos, IM_COL32(235, 245, 255, 255), title.c_str());
		textPos.y += 20.0f;
		if (context.isTexturePreview) {
			drawList->AddText(textPos, IM_COL32(190, 220, 255, 255), "Left drag: pan 2D Texture / Wheel: zoom / R: reset");
		} else {
			drawList->AddText(textPos, IM_COL32(190, 220, 255, 255), "Left drag: pan / Right drag: orbit / Wheel: zoom / R: reset");
		}
		textPos.y += 20.0f;
		drawList->AddText(textPos, IM_COL32(190, 220, 255, 255), "Click outside Edit View or selected shelf card to return.");
		textPos.y += 20.0f;
		drawList->AddText(textPos, IM_COL32(255, 220, 120, 255), context.isTexturePreview
			? "Drag this texture card to Edit View, or use Add Selected, to place it."
			: "Drag this shelf card to Edit View to add it to the scene.");
	} else {
		drawList->AddText(textPos, IM_COL32(235, 245, 255, 255), "Drop model here");
		textPos.y += 20.0f;
		drawList->AddText(textPos, IM_COL32(190, 220, 255, 255), "Release on Edit View to add the model at the cursor.");
		drawList->AddRect(gameViewMin, gameViewMax, IM_COL32(80, 180, 255, 240), 0.0f, 0, 3.0f);
	}

	drawList->PopClipRect();
#else
	static_cast<void>(context);
#endif
}
