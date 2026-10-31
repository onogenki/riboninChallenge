#include "StageMapRepository.h"

#include <filesystem>
#include <utility>

// 選択と読込を一組で扱い、失敗したStageの番号を次の更新へ持ち越しません。
bool StageMapRepository::SelectAndReload(
	int stageId,
	const ApplyLoadedData& applyLoadedData,
	std::string& status)
{
	const StageMapSelection previousSelection = selection_;
	if (!selection_.Select(stageId)) {
		status = "Stage ID must be between 1 and 5.";
		return false;
	}
	try {
		if (Reload(applyLoadedData, status)) {
			return true;
		}
	} catch (...) {
		selection_ = previousSelection;
		throw;
	}
	selection_ = previousSelection;
	return false;
}

// JSONと、存在する場合はCSVを仮読込し、Sceneへの反映失敗時はRepositoryの旧データへ戻します。
bool StageMapRepository::Reload(const ApplyLoadedData& applyLoadedData, std::string& status)
{
	const StageMapSelection::Definition* definition = selection_.GetDefinition();
	if (!definition) {
		status = "Reload failed. No stage definition is selected.";
		return false;
	}
	std::unique_ptr<LevelLoader::LevelData> loadedData =
		LevelLoader::Load(definition->levelFileName);
	if (!loadedData) {
		status = "Reload failed. Current stage was kept.";
		return false;
	}
	MapChipField loadedMapChipField;
	const bool hasMapChipFile = std::filesystem::exists(definition->mapChipFilePath);
	if (hasMapChipFile && !loadedMapChipField.LoadCsv(definition->mapChipFilePath)) {
		status = "Reload failed. " + std::string(definition->mapChipFilePath) + " could not be read.";
		return false;
	}

	std::unique_ptr<LevelLoader::LevelData> previousData = std::move(levelData_);
	MapChipField previousMapChipField = std::move(mapChipField_);
	levelData_ = std::move(loadedData);
	mapChipField_ = std::move(loadedMapChipField);
	try {
		if (!applyLoadedData || !applyLoadedData()) {
			levelData_ = std::move(previousData);
			mapChipField_ = std::move(previousMapChipField);
			status = "Reload failed. Current stage was kept.";
			return false;
		}
	} catch (...) {
		levelData_ = std::move(previousData);
		mapChipField_ = std::move(previousMapChipField);
		throw;
	}

	status = "Reloaded " + std::string(definition->levelFileName) + ".json: " +
		std::to_string(levelData_->objects.size()) + " objects, " +
		(hasMapChipFile
			? std::to_string(mapChipField_.GetChips().size()) + " map chips."
			: "no map chip CSV.");
	return true;
}

// 保存成功時だけ座標系と監視時刻を確定し、失敗時は編集中の座標系を戻します。
bool StageMapRepository::Save(std::string& status)
{
	if (!levelData_) {
		status = "Save failed. No map data is loaded.";
		return false;
	}
	const StageMapSelection::Definition* definition = selection_.GetDefinition();
	if (!definition) {
		status = "Save failed. Check the active stage JSON and its .tmp file.";
		return false;
	}
	const std::string previousCoordinateSystem = levelData_->coordinateSystem;
	levelData_->coordinateSystem = "engine";
	bool saved = false;
	try {
		saved = LevelLoader::Save(definition->levelFileName, *levelData_);
	} catch (...) {
		levelData_->coordinateSystem = previousCoordinateSystem;
		throw;
	}
	if (!saved) {
		levelData_->coordinateSystem = previousCoordinateSystem;
		status = "Save failed. Check the active stage JSON and its .tmp file.";
		return false;
	}
	fileMonitor_.SynchronizeJson();
	status = "Saved " + std::string(definition->levelFileName) + ".json.";
	return true;
}

// Sceneが新StageのObjectを確定した後だけ、監視対象を切り替えます。
void StageMapRepository::StartWatchingCurrentStage()
{
	const StageMapSelection::Definition* definition = selection_.GetDefinition();
	if (definition) {
		fileMonitor_.Start(definition->levelFilePath, definition->mapChipFilePath);
	}
}

// Stageを破棄するときに、LevelDataとCSVだけをまとめて解放します。
void StageMapRepository::Clear()
{
	levelData_.reset();
	mapChipField_ = {};
}
