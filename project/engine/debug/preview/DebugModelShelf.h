#pragma once

#include "SceneEditor.h"
#include <functional>
#include <string>

// DebugScene用のModel Shelfです。
// resources一覧、棚での選択、表示メッセージを所有し、実際の生成とプレビューはSceneへ依頼します。
class DebugModelShelf
{
public:
	struct Context
	{
		// Sceneが所有する追加済みデータの個数です。
		size_t addedModelCount = 0;
		size_t addedTextureCount = 0;
		// Scene固有の生成・削除・プレビュー処理を呼ぶ窓口です。
		std::function<bool(const std::string&)> addModel;
		std::function<bool(const std::string&)> addTexture;
		std::function<bool(const std::string&, float, float)> addModelAtDropPosition;
		std::function<bool(const std::string&, float, float)> addTextureAtDropPosition;
		std::function<void()> clearAdded;
		std::function<void()> afterAdd;
		std::function<bool(const SceneEditor::ShelfEntry&)> previewEntry;
		// Debug表示とPreview操作に使う、Scene側の状態・処理です。
		bool* showCollisionDebug = nullptr;
		bool isPreviewActive = false;
		std::string previewDisplayName;
		std::function<void()> exitPreview;
		std::function<void()> resetPreview;
	};

	// resources内を調べ、棚へ表示するモデルとTextureの一覧を更新します。
	void ScanResources();
	// Model Shelfを描画し、追加・削除・Preview・DropをSceneの窓口へ渡します。
	void Draw(const Context& context);
	// Smoke Testなどが読む、現在の棚一覧です。
	std::vector<SceneEditor::ShelfEntry>& GetEntries();
	const std::vector<SceneEditor::ShelfEntry>& GetEntries() const;
	// Scene終了時に棚の選択と表示メッセージを消去します。
	void Finalize();

private:
	// resourcesから読み込んだ棚一覧・選択項目・直近の処理結果です。
	SceneEditor::ShelfState state_{};
};
