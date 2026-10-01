#include "Stage1.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace
{
	constexpr const char* kStageMapFileName = "stage1";
	constexpr const char* kStageMapChipFilePath = "resources/maps/stage1.csv";
}

// JSONまたはCSVの保存時刻を確認し、変更があった時だけStageデータを読み直します。
void Stage1::UpdateStageMapHotReload()
{
	if (!autoStageMapReload_) {
		return;
	}

	// JSONまたはCSVを保存したフレームだけ、二つのマップデータをまとめて再読込する。
	if (stageMapHotReload_.ConsumeChange() || stageMapChipHotReload_.ConsumeChange()) {
		ReloadStageMap();
	}
}

// stage1.jsonを読み、CSVがある時だけマップチップも実行中のStageデータへ反映します。
bool Stage1::ReloadStageMap()
{
	// 読込に失敗した時は、現在表示中のマップを変更しない
	std::unique_ptr<LevelLoader::LevelData> loadedData =
		LevelLoader::Load(kStageMapFileName);
	if (!loadedData) {
		stageMapReloadStatus_ = "Reload failed. Current stage was kept.";
		return false;
	}
	MapChipField loadedMapChipField;
	const bool hasMapChipFile = std::filesystem::exists(kStageMapChipFilePath);
	if (hasMapChipFile && !loadedMapChipField.LoadCsv(kStageMapChipFilePath)) {
		stageMapReloadStatus_ = "Reload failed. stage1.csv could not be read.";
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
		stageMapReloadStatus_ = "Reload failed. Current stage was kept.";
		return false;
	}

	if (stageMapData_->objects.empty()) {
		selectedStageMapObjectIndex_ = -1;
	} else {
		selectedStageMapObjectIndex_ = std::clamp(
			selectedStageMapObjectIndex_,
			0,
			static_cast<int>(stageMapData_->objects.size()) - 1);
	}

	stageMapReloadStatus_ = "Reloaded stage1.json: " +
		std::to_string(stageMapData_->objects.size()) + " objects, " +
		(hasMapChipFile
			? std::to_string(stageMapChipField_.GetChips().size()) + " map chips."
			: "no map chip CSV.");
	return true;
}

// Edit Viewで変更したstage1.jsonを保存し、直後の再読込を防ぐため監視時刻も同期します。
bool Stage1::SaveStageMap()
{
	if (!stageMapData_) {
		stageMapReloadStatus_ = "Save failed. No map data is loaded.";
		return false;
	}

	stageMapData_->coordinateSystem = "engine";
	if (!LevelLoader::Save(kStageMapFileName, *stageMapData_)) {
		stageMapReloadStatus_ = "Save failed. stage1.json was not changed.";
		return false;
	}

	// 自分で保存した変更を、次のフレームに外部変更として再読込しないよう同期する
	stageMapHotReload_.Synchronize();
	stageMapReloadStatus_ = "Saved stage1.json.";
	return true;
}
