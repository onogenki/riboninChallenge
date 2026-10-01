#include "DebugModelShelf.h"

#include "ImGuiManager.h"
#include <Windows.h>
#include <shellapi.h>

namespace {

// Model ShelfのOpen Resourcesボタンで、resourcesフォルダをExplorerへ表示します。
bool OpenResourcesFolder()
{
	const HINSTANCE result = ShellExecuteW(
		nullptr,
		L"open",
		L"resources",
		nullptr,
		nullptr,
		SW_SHOWNORMAL);
	return reinterpret_cast<INT_PTR>(result) > 32;
}

}

// resources内を調べ、棚へ表示するモデルとTextureの一覧を更新します。
void DebugModelShelf::ScanResources()
{
	SceneEditor::ScanResourceShelf(state_);
}

// Model Shelfを描画し、追加・削除・Preview・DropをSceneの窓口へ渡します。
void DebugModelShelf::Draw(const Context& context)
{
#ifdef USE_IMGUI
	if (!ImGuiManager::GetInstance()->IsEditViewActive()) {
		return;
	}

	SceneEditor::ShelfCallbacks callbacks{};
	callbacks.sceneLabel = "Edit View";
	callbacks.addedModelCount = context.addedModelCount;
	callbacks.addedTextureCount = context.addedTextureCount;
	callbacks.addModel = context.addModel;
	callbacks.addTexture = context.addTexture;
	callbacks.addModelAtDropPosition = context.addModelAtDropPosition;
	callbacks.addTextureAtDropPosition = context.addTextureAtDropPosition;
	callbacks.clearAdded = context.clearAdded;
	callbacks.afterAdd = context.afterAdd;
	callbacks.previewOnDoubleClick = true;
	callbacks.previewEntry = context.previewEntry;
	callbacks.drawExtraToolbar = [&context, this]() {
		if (context.showCollisionDebug) {
			ImGui::SameLine();
			ImGui::Checkbox("Collision Wire", context.showCollisionDebug);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Open Resources")) {
			state_.message = OpenResourcesFolder()
				? "Opened resources folder."
				: "Could not open resources folder.";
		}
	};
	callbacks.drawExtraStatus = [&context]() {
		if (!context.isPreviewActive) {
			return;
		}
		ImGui::TextDisabled("Preview: %s", context.previewDisplayName.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Back to Scene") && context.exitPreview) {
			context.exitPreview();
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset View") && context.resetPreview) {
			context.resetPreview();
		}
	};

	SceneEditor::DrawModelShelf(state_, callbacks);
	// Drop先の判定と枠線表示は共通SceneEditorへ任せ、座標からの追加だけをContextで返します。
	SceneEditor::HandleShelfDropOnEditView(state_, callbacks);
#else
	static_cast<void>(context);
#endif
}

// Smoke Testなどが読む、現在の棚一覧です。
std::vector<SceneEditor::ShelfEntry>& DebugModelShelf::GetEntries()
{
	return state_.entries;
}

// const版の棚一覧取得です。
const std::vector<SceneEditor::ShelfEntry>& DebugModelShelf::GetEntries() const
{
	return state_.entries;
}

// Scene終了時に棚の選択と表示メッセージを消去します。
void DebugModelShelf::Finalize()
{
	state_ = {};
}
