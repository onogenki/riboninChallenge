#pragma once

#include "LevelLoader.h"
#include "SceneEditor.h"
#include <functional>
#include <string>

class StageMapRuntime;

// Stage1のJSONへモデル・Event・Camera Areaを追加／削除するEdit View用のコマンド部品です。
// Stage1はLevelDataと実行中モデルを所有し、このクラスは編集UIとObjectDataの初期値を担当します。
class StageLevelEditor
{
public:
	struct Context
	{
		// 現在のStageが所有する編集対象データと、Player近くへ追加する基準位置です。
		int stageId = 1;
		LevelLoader::LevelData* levelData = nullptr;
		// 描画済みモデルの頂点範囲からBOXの初期形状を求めるため、Runtimeを借ります。
		const StageMapRuntime* mapRuntime = nullptr;
		Vector3 playerPosition{};
		int* selectedObjectIndex = nullptr;
		// 実行中モデルへの反映と状態表示だけを借り、JSON保存はInspectorの明示操作へ任せます。
		std::function<bool(bool rebuildRuntimeObjects)> applyLevelData;
		std::function<void(const std::string&)> setStatus;
	};

	// resourcesを調べ、Stage1 Edit Viewのモデル棚を作ります。
	void Initialize();
	// モデル棚を表示し、モデル追加とEditor追加分の一括削除を受け付けます。
	void DrawModelShelf(const Context& context);
	// Edit Viewへ落としたモデルを、同じ追加処理へ渡します。
	void HandleShelfDropOnEditView(const Context& context);
	// 任意モデル、Sphere、Event対、Camera Area、Path BoxをLevelDataへ追加します。
	bool AddModel(const Context& context, const std::string& fileName);
	bool AddSphere(const Context& context);
	bool AddEventPair(const Context& context);
	bool AddCameraArea(const Context& context);
	bool AddPathBox(const Context& context);
	// 選択中のEditor追加モデルだけを改名し、失敗した再構築では旧名へ戻します。
	bool RenameSelectedObject(const Context& context, const std::string& requestedName);
	// 選択中の通常モデルへBOX判定を付け、反映失敗時には元のデータへ戻します。
	bool AddBoxCollider(const Context& context);
	// 選択中の削除可能なObjectを消し、再構築失敗時はデータと選択を戻します。
	bool RemoveSelectedObject(const Context& context);
	// Editor追加モデルだけをメモリから消し、再構築の結果を表示用の文章で返します。
	std::string ClearEditorAddedObjects(const Context& context);
	// Scene終了時にShelfの前回選択を破棄します。
	void Finalize();
	// Stage番号を変えたとき、resourcesの一覧は保って前Stageの選択と通知だけ消します。
	void ResetForStage();

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
	// 現在のStageで使うresources一覧と選択状態です。
	SceneEditor::ShelfState shelfState_{};
};
