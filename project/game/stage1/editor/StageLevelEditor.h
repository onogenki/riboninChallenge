#pragma once

#include "LevelLoader.h"
#include "SceneEditor.h"
#include <functional>
#include <string>

// Stage1のJSONへモデル・Event・Camera Areaを追加／削除するEdit View用のコマンド部品です。
// Stage1はLevelDataと実行中モデルを所有し、このクラスは編集UIとObjectDataの初期値を担当します。
class StageLevelEditor
{
public:
	struct Context
	{
		// Stage1が所有する編集対象データと、Player近くへ追加する基準位置です。
		LevelLoader::LevelData* levelData = nullptr;
		Vector3 playerPosition{};
		int* selectedObjectIndex = nullptr;
		// Stage1側の実行中モデル再構築・JSON保存・状態表示を呼ぶ窓口です。
		std::function<bool(bool rebuildRuntimeObjects)> applyLevelData;
		std::function<bool()> saveLevelData;
		std::function<void(const std::string&)> setStatus;
	};

	// resourcesを調べ、Stage1 Edit Viewのモデル棚を作ります。
	void Initialize();
	// モデル棚を表示し、モデル追加とEditor追加分の一括削除を受け付けます。
	void DrawModelShelf(const Context& context);
	// Edit Viewへ落としたモデルを、同じ追加処理へ渡します。
	void HandleShelfDropOnEditView(const Context& context);
	// 任意モデル、Sphere、Event対、Camera Area、Path SphereをLevelDataへ追加します。
	bool AddModel(const Context& context, const std::string& fileName);
	bool AddSphere(const Context& context);
	bool AddEventPair(const Context& context);
	bool AddCameraArea(const Context& context);
	bool AddPathSphere(const Context& context);
	// 選択中の削除可能なObjectをLevelDataから削除します。
	bool RemoveSelectedObject(const Context& context);
	// Editorから追加した通常モデルだけを全削除し、JSON保存まで行います。
	void ClearEditorAddedObjects(const Context& context);
	// Scene終了時にShelfの前回選択を破棄します。
	void Finalize();

private:
	// 既存Object名と重ならない連番の名前を作ります。
	static std::string MakeUniqueName(
		const LevelLoader::LevelData& levelData,
		const std::string& prefix);
	// 一つのObjectDataを追加後、実行中モデルへ反映し、失敗時は追加を取り消します。
	bool CommitAddedObject(
		const Context& context,
		LevelLoader::ObjectData objectData,
		const std::string& successMessage,
		const std::string& failureMessage);
	// Stage1 Edit Viewのresources一覧と選択状態です。
	SceneEditor::ShelfState shelfState_{};
};
