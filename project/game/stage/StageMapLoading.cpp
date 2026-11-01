#include "Stage.h"

#include "Player.h"

// Sceneを切り替えずに次のLevelを読み、成功後だけ監視対象とPlayer開始位置を更新します。
bool Stage::LoadStage(int stageId, bool resetPlayerPosition)
{
	// Repositoryが選択・読込を一組で確定し、失敗した番号とデータを残しません。
	std::string reloadStatus;
	if (!stageMapRepository_.SelectAndReload(
		stageId,
		[this]() { return ApplyStageMapData(true); },
		reloadStatus)) {
		stageEditor_.SetReloadStatus(reloadStatus);
		return false;
	}
	stageEditor_.ResetForStage(*stageMapRepository_.GetLevelData());
	if (resetPlayerPosition && player_) {
		const Vector3 playerStartPosition = GetStagePlayerStartPosition();
		player_->SetPosition(playerStartPosition);
	}
	stageEditor_.SetReloadStatus(reloadStatus);
	// 前のStageで開いたDoorや危険Lightの時刻を持ち越さず、新しい配置を初期状態から動かします。
	lightPuzzle_.ResetProgress();
	hazardLights_.Reset();
	// 別Stageへ切り替えた時だけ死亡回数と演出を初期化し、前Stageの失敗を持ち越しません。
	lightDeath_.Reset();
	isLeavingForGameOver_ = false;
	if (player_) {
		player_->GetObject().SetOpacity(1.0f);
		player_->GetObject().SetSpecularGlow(1.0f, 0.0f);
		stageLightWarning_.Reset(*player_);
	}
	stageMapRepository_.StartWatchingCurrentStage();
	return true;
}

// Stage固有Behaviorから呼べるよう、要求だけを記録してUpdate中の安全な順番で切り替えます。
bool Stage::RequestStage(int stageId)
{
	if (startsWithTemporaryTitle_) {
		return false;
	}
	return stageMapRepository_.RequestStage(stageId);
}

// 更新途中に所有物を入れ替えないよう、予約されたStage変更をUpdateの先頭で一度だけ反映します。
void Stage::ApplyRequestedStageChange()
{
	const int requestedStageId = stageMapRepository_.TakeRequestedStageId();
	if (requestedStageId == 0 || requestedStageId == stageMapRepository_.GetStageId()) {
		return;
	}
	LoadStage(requestedStageId, true);
}

// JSONまたはCSVの保存時刻を確認し、変更があった時だけStageデータを読み直します。
void Stage::UpdateStageMapHotReload()
{
	if (!stageEditor_.IsAutoMapReloadEnabled()) {
		return;
	}

	// JSONまたはCSVを保存したフレームだけ、二つのマップデータをまとめて再読込する。
	if (stageMapRepository_.ConsumeFileChange()) {
		ReloadStageMap();
	}
}

// RepositoryがJSON・CSVを仮読込した後、床・鏡・Cameraの反映に成功した時だけ採用します。
bool Stage::ReloadStageMap(bool resetPlayerPosition)
{
	std::string reloadStatus;
	if (!stageMapRepository_.Reload(
		[this]() { return ApplyStageMapData(true); },
		reloadStatus)) {
		stageEditor_.SetReloadStatus(reloadStatus);
		return false;
	}
	stageEditor_.ClampSelectedObjectIndex(stageMapRepository_.GetLevelData()->objects.size());
	stageEditor_.ResetHistory(*stageMapRepository_.GetLevelData());
	if (resetPlayerPosition && player_) {
		const Vector3 playerStartPosition = GetStagePlayerStartPosition();
		player_->SetPosition(playerStartPosition);
	}

	stageEditor_.SetReloadStatus(reloadStatus);
	return true;
}

// Edit Viewの保存要求だけをRepositoryへ渡し、結果をEditorに表示します。
bool Stage::SaveStageMap()
{
	std::string saveStatus;
	const bool saved = stageMapRepository_.Save(saveStatus);
	stageEditor_.SetReloadStatus(saveStatus);
	return saved;
}
