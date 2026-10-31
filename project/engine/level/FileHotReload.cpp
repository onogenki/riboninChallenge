#include "FileHotReload.h"

#include <system_error>

namespace Engine
{
// 監視先を切り替えた直後の時刻を基準にして、初回確認を変更扱いにしません。
void FileHotReload::SetFilePath(const std::string& filePath)
{
	filePath_ = filePath;
	Synchronize();
}

// 存在と更新時刻の差を一度だけ通知し、保存直後の再読込を制御します。
bool FileHotReload::ConsumeChange()
{
	if (filePath_.empty()) {
		return false;
	}

	std::error_code error;
	const bool existsNow = std::filesystem::exists(filePath_, error);
	if (error) {
		return false;
	}

	// 削除・新規作成もファイルの変更として通知します。
	if (existsNow != existed_) {
		existed_ = existsNow;
		hasWriteTime_ = false;
		if (existsNow) {
			lastWriteTime_ = std::filesystem::last_write_time(filePath_, error);
			hasWriteTime_ = !error;
		}
		return true;
	}

	if (!existsNow) {
		return false;
	}

	const std::filesystem::file_time_type currentWriteTime =
		std::filesystem::last_write_time(filePath_, error);
	if (error) {
		return false;
	}

	if (!hasWriteTime_ || currentWriteTime != lastWriteTime_) {
		lastWriteTime_ = currentWriteTime;
		hasWriteTime_ = true;
		return true;
	}

	return false;
}

// ゲーム内の保存後に基準時刻を更新し、自分の保存を再読込しません。
void FileHotReload::Synchronize()
{
	if (filePath_.empty()) {
		existed_ = false;
		hasWriteTime_ = false;
		return;
	}

	std::error_code error;
	existed_ = std::filesystem::exists(filePath_, error);
	if (error || !existed_) {
		existed_ = false;
		hasWriteTime_ = false;
		return;
	}

	lastWriteTime_ = std::filesystem::last_write_time(filePath_, error);
	hasWriteTime_ = !error;
}
} // namespace Engine
