#pragma once

#include <filesystem>
#include <string>

// 外部ファイルの更新日時を監視し、保存された瞬間を一度だけ通知します。
class FileHotReload
{
public:
	// 監視するファイルを設定し、現在の更新日時を基準として記録します。
	void SetFilePath(const std::string& filePath);
	// 前回確認した時刻からファイルが変化していれば、一度だけ true を返します。
	bool ConsumeChange();
	// 手動再読込後などに、現在の更新日時を新しい基準として記録します。
	void Synchronize();

	const std::string& GetFilePath() const { return filePath_; }

private:
	std::string filePath_;
	std::filesystem::file_time_type lastWriteTime_{};
	bool existed_ = false;
	bool hasWriteTime_ = false;
};
