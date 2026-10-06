#pragma once
#include <string>

namespace Logger
{
	// 起動時に決めたログの保存先を設定し、以後のLogで使用します。
	void SetLogPath(const std::string& filePath);

	// デバッグ出力に記録し、保存先が設定済みならファイルにも追記します。
	void Log(const std::string& message);
}
