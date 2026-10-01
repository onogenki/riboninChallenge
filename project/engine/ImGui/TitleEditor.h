#pragma once

#include "SceneEditor.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Camera;

// Title画面のModel Shelf・Inspector・Edit ViewをまとめるUI部品です。
// モデル・SpriteはTitleObjectManager、Camera・LightはTitleSceneが所有します。
// TitleEditorはそれらをContextで借り、選択状態とUI操作だけを所有します。
class TitleEditor
{
public:
	struct Context
	{
		// SceneのCamera・Lightと、Managerのモデル・Spriteを借ります。TitleEditorは所有しません。
		Camera* camera = nullptr;
		std::vector<std::unique_ptr<Object3d>>* normalObjects = nullptr;
		std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		std::vector<std::unique_ptr<Sprite>>* sprites = nullptr;
		Object3d::DirectionalLight* directionalLight = nullptr;
		Object3d::PointLight* pointLight = nullptr;
		Object3d::SpotLight* spotLight = nullptr;
		// Title開始時から置かれている編集保護対象の数です。
		size_t baseNormalObjectCount = 0;
		size_t baseAnimationObjectCount = 0;
		size_t baseSpriteCount = 0;
		// Sceneのルールに従ってモデル・Textureを追加・削除する窓口です。
		std::function<bool(const std::string&)> addModel;
		std::function<bool(const std::string&)> addTexture;
		std::function<void()> clearAdded;
	};

	// resources内を調べ、Title用Model Shelfの一覧を作ります。
	void ScanResourceShelf();
	// Model Shelf・Inspector・3D/2D Edit Viewを表示し、選択状態を更新します。
	void Draw(const Context& context);
	// Title開始時のSpriteを、最初からInspector編集対象にします。
	void SelectSprite(size_t index);
	// Scene終了時にShelfと選択状態を初期状態へ戻します。
	void Finalize();

private:
	// Model Shelfを表示し、モデル・Textureの追加や一括削除をSceneの窓口へ渡します。
	void DrawModelShelf(const Context& context);
	// 選択中のTitleモデル・Sprite・Lightを編集するInspectorを表示します。
	void DrawInspector(const Context& context);
	// 3Dモデルを選択・移動・回転・拡大できるTitle用Viewportを表示します。
	void DrawModelViewport(const Context& context);
	// Spriteを選択・移動・回転・拡縮できるTitle用Viewportを表示します。
	void DrawSpriteViewport(const Context& context);
	// Model ShelfからEdit Viewへ落とした項目を、Sceneの追加関数へ渡します。
	void HandleShelfDrop(const Context& context);
	// Model ShelfとDrag & Dropの共通処理として、追加後のモデルをInspector選択にします。
	bool AddModelAndSelect(const Context& context, const std::string& fileName);
	// Model ShelfとDrag & Dropの共通処理として、追加後のSpriteをInspector選択にします。
	bool AddTextureAndSelect(const Context& context, const std::string& textureFilePath);
	// 現在の選択状態を解除します。
	void ClearSelection();
	// 追加成功直後に、通常モデルまたはAnimationモデルをInspectorへ選択します。
	void SelectAddedModel(const Context& context, size_t normalCountBefore, size_t animationCountBefore);

	// resourcesから見つけたモデル・Textureの表示一覧です。
	SceneEditor::ShelfState shelfState_{};
	// 3Dモデルを編集するViewのCamera・選択状態です。
	SceneEditor::ViewportState modelViewportState_{};
	// Spriteを編集するViewの選択状態です。
	SceneEditor::SpriteViewportState spriteViewportState_{};
	// Inspectorで選択している3Dモデル・Spriteの状態です。
	bool hasSelectedObject_ = false;
	bool selectedObjectIsAnimation_ = false;
	size_t selectedObjectIndex_ = 0;
	bool hasSelectedSprite_ = false;
	size_t selectedSpriteIndex_ = 0;
	// 追加直後にInspectorへ選択状態を渡す残りフレーム数です。
	int inspectorAutoSelectModelFrames_ = 0;
	int inspectorAutoSelectSpriteFrames_ = 0;
};
