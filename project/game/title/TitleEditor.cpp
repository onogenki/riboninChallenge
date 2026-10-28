#include "TitleEditor.h"

#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include <algorithm>
#include <cstddef>

// SceneEditor共通処理を使い、Title専用Shelfのresources一覧を作ります。
void TitleEditor::ScanResourceShelf()
{
	SceneEditor::ScanResourceShelf(shelfState_);
}

// TitleSceneが渡したデータを使い、Edit Viewで必要なUIを一か所から表示します。
void TitleEditor::Draw(const Context& context)
{
	if (!ImGuiManager::GetInstance()->IsEditViewActive()) {
		return;
	}
	if (!context.normalObjects || !context.animationObjects || !context.sprites ||
		!context.directionalLight || !context.pointLight || !context.spotLight) {
		return;
	}

	DrawModelShelf(context);
	DrawInspector(context);
	HandleShelfDrop(context);
	DrawModelViewport(context);
	DrawSpriteViewport(context);
}

// Title開始時に置かれたSpriteを、InspectorとSprite Viewportの選択対象にします。
void TitleEditor::SelectSprite(size_t index)
{
	hasSelectedSprite_ = true;
	selectedSpriteIndex_ = index;
	hasSelectedObject_ = false;
}

// Scene終了後に古いShelf・選択番号を次のTitle表示へ持ち込まないようにします。
void TitleEditor::Finalize()
{
	shelfState_.entries.clear();
	shelfState_.selectedEntry.clear();
	shelfState_.message.clear();
	modelViewportState_ = {};
	spriteViewportState_ = {};
	ClearSelection();
}

// Model Shelfの追加・一括削除要求を、TitleSceneの生成ルールへ渡します。
void TitleEditor::DrawModelShelf(const Context& context)
{
	SceneEditor::ShelfCallbacks callbacks{};
	callbacks.sceneLabel = "Title";
	callbacks.addedModelCount =
		(context.normalObjects->size() > context.baseNormalObjectCount
			? context.normalObjects->size() - context.baseNormalObjectCount
			: 0) +
		(context.animationObjects->size() > context.baseAnimationObjectCount
			? context.animationObjects->size() - context.baseAnimationObjectCount
			: 0);
	callbacks.addedTextureCount = context.sprites->size() > context.baseSpriteCount
		? context.sprites->size() - context.baseSpriteCount
		: 0;
	callbacks.addModel = [this, &context](const std::string& fileName)
	{
		return AddModelAndSelect(context, fileName);
	};
	callbacks.addTexture = [this, &context](const std::string& textureFilePath)
	{
		return AddTextureAndSelect(context, textureFilePath);
	};
	callbacks.clearAdded = [this, &context]()
	{
		if (context.clearAdded) {
			context.clearAdded();
		}
		ClearSelection();
	};
	SceneEditor::DrawModelShelf(shelfState_, callbacks);
}

// Inspectorへモデル・Sprite・Lightを渡し、削除時の選択番号も安全に更新します。
void TitleEditor::DrawInspector(const Context& context)
{
	const int forcedSpriteIndex = hasSelectedSprite_ &&
		inspectorAutoSelectSpriteFrames_ > 0 &&
		selectedSpriteIndex_ < context.sprites->size()
		? static_cast<int>(selectedSpriteIndex_)
		: -1;
	if (inspectorAutoSelectSpriteFrames_ > 0) {
		--inspectorAutoSelectSpriteFrames_;
	}

	SceneEditor::InspectorOptions options{};
	options.description = "Adjust TitleScene models and 2D textures added from Model Shelf.";
	options.sprites = context.sprites;
	options.normalObjects = context.normalObjects;
	options.animationObjects = context.animationObjects;
	options.directionalLight = context.directionalLight;
	options.pointLight = context.pointLight;
	options.spotLight = context.spotLight;
	options.addedSpriteCount = context.sprites->size() > context.baseSpriteCount
		? context.sprites->size() - context.baseSpriteCount
		: 0;
	options.protectedSpriteCount = context.baseSpriteCount;
	options.protectedNormalObjectCount = context.baseNormalObjectCount;
	options.protectedAnimationObjectCount = context.baseAnimationObjectCount;
	options.forcedSpriteIndex = forcedSpriteIndex;
	options.forcedNormalIndex = hasSelectedObject_ && !selectedObjectIsAnimation_ &&
		inspectorAutoSelectModelFrames_ > 0
		? static_cast<int>(selectedObjectIndex_)
		: -1;
	options.forcedAnimationIndex = hasSelectedObject_ && selectedObjectIsAnimation_ &&
		inspectorAutoSelectModelFrames_ > 0
		? static_cast<int>(selectedObjectIndex_)
		: -1;
	options.selectSpriteTab = forcedSpriteIndex >= 0;
	options.selectModelTab = options.forcedNormalIndex >= 0 || options.forcedAnimationIndex >= 0;
	options.removeSprite = [this, &context](size_t index)
	{
		if (index >= context.sprites->size() || index < context.baseSpriteCount) {
			return;
		}
		DirectXCommon::GetInstance()->WaitForGPU();
		context.sprites->erase(context.sprites->begin() + static_cast<std::ptrdiff_t>(index));
		if (context.sprites->empty()) {
			ClearSelection();
			return;
		}
		SelectSprite((std::min)(index, context.sprites->size() - 1));
		inspectorAutoSelectSpriteFrames_ = 2;
	};
	SceneEditor::DrawInspector(options);
	if (inspectorAutoSelectModelFrames_ > 0) {
		--inspectorAutoSelectModelFrames_;
	}
}

// 通常・Animationモデルを一つのViewport一覧に並べ、選択を元の配列番号へ戻します。
void TitleEditor::DrawModelViewport(const Context& context)
{
#ifdef USE_IMGUI
	SceneEditor::ViewportOptions options{};
	options.camera = context.camera;
	for (size_t index = 0; index < context.normalObjects->size(); ++index) {
		Object3d* object = (*context.normalObjects)[index].get();
		const std::string name = object && !object->GetModelName().empty()
			? object->GetModelName()
			: "Title Model";
		options.objects.push_back({ name + " [" + std::to_string(index) + "]", object });
	}
	const size_t animationOffset = options.objects.size();
	for (size_t index = 0; index < context.animationObjects->size(); ++index) {
		Object3d* object = (*context.animationObjects)[index].get();
		const std::string name = object && !object->GetModelName().empty()
			? object->GetModelName()
			: "Title Animation";
		options.objects.push_back({ name + " [Animation " + std::to_string(index) + "]", object });
	}

	modelViewportState_.selectedIndex = hasSelectedObject_
		? static_cast<int>(selectedObjectIsAnimation_
			? animationOffset + selectedObjectIndex_
			: selectedObjectIndex_)
		: -1;
	options.onSelectionChanged = [this, animationOffset](int index)
	{
		if (index < 0) {
			hasSelectedObject_ = false;
			return;
		}
		hasSelectedObject_ = true;
		hasSelectedSprite_ = false;
		selectedObjectIsAnimation_ = static_cast<size_t>(index) >= animationOffset;
		selectedObjectIndex_ = selectedObjectIsAnimation_
			? static_cast<size_t>(index) - animationOffset
			: static_cast<size_t>(index);
		inspectorAutoSelectModelFrames_ = 2;
	};
	SceneEditor::DrawViewportEditor(modelViewportState_, options);
#endif
}

// Sprite一覧をViewportへ並べ、クリックしたSpriteをInspector編集対象にします。
void TitleEditor::DrawSpriteViewport(const Context& context)
{
#ifdef USE_IMGUI
	SceneEditor::SpriteViewportOptions options{};
	for (size_t index = 0; index < context.sprites->size(); ++index) {
		options.sprites.push_back({
			"Title Sprite [" + std::to_string(index) + "]",
			(*context.sprites)[index].get(),
		});
	}
	spriteViewportState_.selectedIndex = hasSelectedSprite_ && selectedSpriteIndex_ < context.sprites->size()
		? static_cast<int>(selectedSpriteIndex_)
		: -1;
	options.onSelectionChanged = [this](int index)
	{
		if (index < 0) {
			hasSelectedSprite_ = false;
			return;
		}
		SelectSprite(static_cast<size_t>(index));
		inspectorAutoSelectSpriteFrames_ = 2;
		inspectorAutoSelectModelFrames_ = 0;
	};
	SceneEditor::DrawSpriteViewportEditor(spriteViewportState_, options);
#endif
}

// Edit ViewへのDrag & Dropを、Model Shelfと同じ追加処理へ流します。
void TitleEditor::HandleShelfDrop(const Context& context)
{
	SceneEditor::ShelfCallbacks callbacks{};
	callbacks.sceneLabel = "Title";
	callbacks.addModel = [this, &context](const std::string& fileName)
	{
		return AddModelAndSelect(context, fileName);
	};
	callbacks.addTexture = [this, &context](const std::string& textureFilePath)
	{
		return AddTextureAndSelect(context, textureFilePath);
	};
	SceneEditor::HandleShelfDropOnEditView(shelfState_, callbacks);
}

// Model ShelfとDrag & Dropで共通の、追加後にInspectorを選択する処理です。
bool TitleEditor::AddModelAndSelect(const Context& context, const std::string& fileName)
{
	if (!context.addModel || !context.normalObjects || !context.animationObjects) {
		return false;
	}

	const size_t normalCountBefore = context.normalObjects->size();
	const size_t animationCountBefore = context.animationObjects->size();
	if (!context.addModel(fileName)) {
		return false;
	}
	SelectAddedModel(context, normalCountBefore, animationCountBefore);
	return true;
}

// Model ShelfとDrag & Dropで共通の、追加後にSpriteをInspector選択する処理です。
bool TitleEditor::AddTextureAndSelect(
	const Context& context,
	const std::string& textureFilePath)
{
	if (!context.addTexture || !context.sprites ||
		!context.addTexture(textureFilePath) || context.sprites->empty()) {
		return false;
	}

	SelectSprite(context.sprites->size() - 1);
	inspectorAutoSelectSpriteFrames_ = 2;
	return true;
}

// モデルとSpriteのどちらも選択されていない初期状態へ戻します。
void TitleEditor::ClearSelection()
{
	hasSelectedObject_ = false;
	selectedObjectIsAnimation_ = false;
	selectedObjectIndex_ = 0;
	hasSelectedSprite_ = false;
	selectedSpriteIndex_ = 0;
	inspectorAutoSelectModelFrames_ = 0;
	inspectorAutoSelectSpriteFrames_ = 0;
}

// モデル追加後に増えた一覧を調べ、追加した最後のモデルをInspector編集対象にします。
void TitleEditor::SelectAddedModel(
	const Context& context,
	size_t normalCountBefore,
	size_t animationCountBefore)
{
	hasSelectedSprite_ = false;
	hasSelectedObject_ = true;
	if (context.animationObjects->size() > animationCountBefore) {
		selectedObjectIsAnimation_ = true;
		selectedObjectIndex_ = context.animationObjects->size() - 1;
	} else if (context.normalObjects->size() > normalCountBefore) {
		selectedObjectIsAnimation_ = false;
		selectedObjectIndex_ = context.normalObjects->size() - 1;
	} else {
		hasSelectedObject_ = false;
		return;
	}
	inspectorAutoSelectModelFrames_ = 2;
}
