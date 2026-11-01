#pragma once

#include "LevelLoader.h"
#include <functional>
#include <optional>

// StageEditorが所有する、現在Stageの直前一手順だけのJSON編集履歴です。
// PlayerStartと通常Cameraの操作を除き、ドラッグ中の連続変更を一手順へまとめます。
class StageEditHistory
{
public:
	// 前回観測した編集データとの差分を記録し、操作中の連続変更を一手順へまとめます。
	void Observe(const LevelLoader::LevelData& levelData, int selectedObjectIndex, bool isEditingContinuously);
	// 実行中のStageへ復元できた場合だけ、履歴の位置を進めます。
	bool Undo(LevelLoader::LevelData& levelData, int& selectedObjectIndex, const std::function<bool()>& apply);
	bool Redo(LevelLoader::LevelData& levelData, int& selectedObjectIndex, const std::function<bool()>& apply);
	// Stage切替・JSON再読込・Scene終了で、別のデータへ古い履歴を適用しないよう消します。
	void Clear();
	bool CanUndo() const { return undo_.has_value(); }
	bool CanRedo() const { return redo_.has_value(); }

private:
	struct Snapshot
	{
		LevelLoader::LevelData levelData;
		int selectedObjectIndex = -1;
	};

	// PlayerStartと保存形式の違いを保持し、JSON編集分だけ復元してからStageへ反映します。
	bool Restore(
		std::optional<Snapshot>& source,
		std::optional<Snapshot>& destination,
		LevelLoader::LevelData& levelData,
		int& selectedObjectIndex,
		const std::function<bool()>& apply);

	std::optional<Snapshot> observed_;
	std::optional<Snapshot> undo_;
	std::optional<Snapshot> redo_;
	bool isContinuousEdit_ = false;
};
