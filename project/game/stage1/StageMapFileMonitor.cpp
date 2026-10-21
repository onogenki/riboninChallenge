#include "StageMapFileMonitor.h"

// Stage切替の読込成功後だけ監視先を変え、失敗したStageのファイルを見ないようにします。
void StageMapFileMonitor::Start(const std::string& levelFilePath, const std::string& mapChipFilePath)
{
	jsonFile_.SetFilePath(levelFilePath);
	mapChipFile_.SetFilePath(mapChipFilePath);
	jsonFile_.Synchronize();
	mapChipFile_.Synchronize();
}

// JSONとCSVを毎回この順に確認し、同時保存でも次フレームへ変更を持ち越さないようにします。
bool StageMapFileMonitor::ConsumeChange()
{
	const bool jsonChanged = jsonFile_.ConsumeChange();
	const bool mapChipChanged = mapChipFile_.ConsumeChange();
	return jsonChanged || mapChipChanged;
}

// 自分で保存したJSONを、次のUpdateで外部変更として扱わないようにします。
void StageMapFileMonitor::SynchronizeJson()
{
	jsonFile_.Synchronize();
}
