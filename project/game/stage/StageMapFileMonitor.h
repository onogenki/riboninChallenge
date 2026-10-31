#pragma once

#include "FileHotReload.h"
#include <string>

// 選択中StageのJSONとCSVだけを監視し、Sceneへ変更の有無を一度だけ通知します。
class StageMapFileMonitor
{
public:
	// 読込に成功したStageのファイルを監視対象にし、現在の保存時刻を基準にします。
	void Start(const std::string& levelFilePath, const std::string& mapChipFilePath);
	// JSONを先に確認し、どちらかが変化した時だけtrueを返します。
	bool ConsumeChange();
	// JSONをゲーム内から保存・手動再読込した後、自己再読込を防ぐため時刻を合わせます。
	void SynchronizeJson();

	const std::string& GetJsonFilePath() const { return jsonFile_.GetFilePath(); }

private:
	Engine::FileHotReload jsonFile_;
	Engine::FileHotReload mapChipFile_;
};
