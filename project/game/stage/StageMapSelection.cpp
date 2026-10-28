#include "StageMapSelection.h"

// Stage IDごとのJSON・CSV対応を一箇所に置き、Scene登録をStage番号ごとに増やしません。
const StageMapSelection::Definition* StageMapSelection::FindDefinition(int stageId)
{
	static constexpr Definition kStageDefinitions[] = {
		{ 1, "stage1", "resources/levels/stage1.json", "resources/maps/stage1.csv" },
		{ 2, "stage2", "resources/levels/stage2.json", "resources/maps/stage2.csv" },
		{ 3, "stage3", "resources/levels/stage3.json", "resources/maps/stage3.csv" },
		{ 4, "stage4", "resources/levels/stage4.json", "resources/maps/stage4.csv" },
		{ 5, "stage5", "resources/levels/stage5.json", "resources/maps/stage5.csv" },
	};
	for (const Definition& definition : kStageDefinitions) {
		if (definition.id == stageId) {
			return &definition;
		}
	}
	return nullptr;
}

// 読込開始時にだけ選択を進め、未登録IDで現在のStageを壊さないようにします。
bool StageMapSelection::Select(int stageId)
{
	const Definition* definition = FindDefinition(stageId);
	if (!definition) {
		return false;
	}
	stageId_ = stageId;
	definition_ = definition;
	return true;
}

// Stageの更新途中でモデルを入れ替えないよう、有効なIDの要求だけを保留します。
bool StageMapSelection::Request(int stageId)
{
	if (!FindDefinition(stageId)) {
		return false;
	}
	requestedStageId_ = stageId;
	return true;
}

// 予約を一度消費し、失敗時に同じ要求を毎フレーム繰り返さないようにします。
int StageMapSelection::TakeRequestedStageId()
{
	const int requestedStageId = requestedStageId_;
	requestedStageId_ = 0;
	return requestedStageId;
}
