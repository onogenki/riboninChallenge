#include "Stage1.h"

#include "Player.h"
#include <filesystem>
#include <utility>

// Sceneを切り替えずに次のLevelを読み、成功後だけ監視対象とPlayer開始位置を更新します。
bool Stage::LoadStage(int stageId, bool resetPlayerPosition)
{
	// ファイル名と切替予約の所有者だけを先に選び、読込失敗時は元の選択を戻します。
	const StageMapSelection previousSelection = stageMapSelection_;
	if (!stageMapSelection_.Select(stageId)) {
		stageEditor_.SetReloadStatus("Stage ID must be between 1 and 5.");
		return false;
	}
	if (!ReloadStageMap(resetPlayerPosition)) {
		stageMapSelection_ = previousSelection;
		return false;
	}
	// 前のStageで開いたDoorや危険Lightの時刻を持ち越さず、新しい配置を初期状態から動かします。
	lightPuzzle_.ResetProgress();
	hazardLights_.Reset();
	const StageMapSelection::Definition* definition = stageMapSelection_.GetDefinition();
	stageMapFileMonitor_.Start(definition->levelFilePath, definition->mapChipFilePath);
	return true;
}

// Stage固有Behaviorから呼べるよう、要求だけを記録してUpdate中の安全な順番で切り替えます。
bool Stage::RequestStage(int stageId)
{
	if (startsWithTemporaryTitle_) {
		return false;
	}
	return stageMapSelection_.Request(stageId);
}

// 更新途中に所有物を入れ替えないよう、予約されたStage変更をUpdateの先頭で一度だけ反映します。
void Stage::ApplyRequestedStageChange()
{
	const int requestedStageId = stageMapSelection_.TakeRequestedStageId();
	if (requestedStageId == 0 || requestedStageId == stageMapSelection_.GetStageId()) {
		return;
	}
	LoadStage(requestedStageId, true);
}

// JSONまたはCSVの保存時刻を確認し、変更があった時だけStageデータを読み直します。
void Stage1::UpdateStageMapHotReload()
{
	if (!stageEditor_.IsAutoMapReloadEnabled()) {
		return;
	}

	// JSONまたはCSVを保存したフレームだけ、二つのマップデータをまとめて再読込する。
	if (stageMapFileMonitor_.ConsumeChange()) {
		ReloadStageMap();
	}
}

// stage1.jsonを読み、CSVがある時だけマップチップも実行中のStageデータへ反映します。
bool Stage1::ReloadStageMap(bool resetPlayerPosition)
{
	const StageMapSelection::Definition* definition = stageMapSelection_.GetDefinition();
	if (!definition) {
		stageEditor_.SetReloadStatus("Reload failed. No stage definition is selected.");
		return false;
	}
	// 読込に失敗した時は、現在表示中のマップを変更しない
	std::unique_ptr<LevelLoader::LevelData> loadedData =
		LevelLoader::Load(definition->levelFileName);
	if (!loadedData) {
		stageEditor_.SetReloadStatus("Reload failed. Current stage was kept.");
		return false;
	}
	MapChipField loadedMapChipField;
	const bool hasMapChipFile = std::filesystem::exists(definition->mapChipFilePath);
	if (hasMapChipFile && !loadedMapChipField.LoadCsv(definition->mapChipFilePath)) {
		stageEditor_.SetReloadStatus("Reload failed. " + std::string(definition->mapChipFilePath) + " could not be read.");
		return false;
	}

	// 新しいJSONとCSVを仮に設定し、全オブジェクトを作れた場合だけ確定する。
	std::unique_ptr<LevelLoader::LevelData> previousData = std::move(stageMapData_);
	MapChipField previousMapChipField = std::move(stageMapChipField_);
	stageMapData_ = std::move(loadedData);
	stageMapChipField_ = std::move(loadedMapChipField);
	if (!ApplyStageMapData(true)) {
		stageMapData_ = std::move(previousData);
		stageMapChipField_ = std::move(previousMapChipField);
		stageEditor_.SetReloadStatus("Reload failed. Current stage was kept.");
		return false;
	}

	stageEditor_.ClampSelectedObjectIndex(stageMapData_->objects.size());
	if (resetPlayerPosition && player_) {
		const Vector3 playerStartPosition = hasStagePlayerStart_
			? stagePlayerStartPosition_
			: Vector3{ 0.0f, -0.8f, 5.0f };
		player_->SetPosition(playerStartPosition);
	}

	stageEditor_.SetReloadStatus("Reloaded " + std::string(definition->levelFileName) + ".json: " +
		std::to_string(stageMapData_->objects.size()) + " objects, " +
		(hasMapChipFile
			? std::to_string(stageMapChipField_.GetChips().size()) + " map chips."
			: "no map chip CSV."));
	return true;
}

// Edit Viewのstage1.jsonを保存し、成功時だけ監視時刻を同期して自己再読込を防ぎます。
bool Stage1::SaveStageMap()
{
	if (!stageMapData_) {
		stageEditor_.SetReloadStatus("Save failed. No map data is loaded.");
		return false;
	}

	stageMapData_->coordinateSystem = "engine";
	const StageMapSelection::Definition* definition = stageMapSelection_.GetDefinition();
	// 定義がない時は保存を呼ばず、保存失敗と同じ状態表示で安全に戻します。
	if (!definition) {
		stageEditor_.SetReloadStatus("Save failed. Check the active stage JSON and its .tmp file.");
		return false;
	}
	if (!LevelLoader::Save(definition->levelFileName, *stageMapData_)) {
		stageEditor_.SetReloadStatus("Save failed. Check the active stage JSON and its .tmp file.");
		return false;
	}

	// 自分で保存した変更を、次のフレームに外部変更として再読込しないよう同期する
	stageMapFileMonitor_.SynchronizeJson();
	stageEditor_.SetReloadStatus("Saved " + std::string(definition->levelFileName) + ".json.");
	return true;
}
