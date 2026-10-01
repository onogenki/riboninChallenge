#include "Logger.h"
#include <format>
#include <fstream>
#include <Windows.h>

namespace Logger
{
	// ファイルパスを保持する変数（このファイル内からのみアクセス可能）
	static std::string gLogPath;

	void SetLogPath(const std::string& filePath)
	{
		gLogPath = filePath;
	}

	void Log(const std::string& message)
	{
		// 自分のログだとすぐわかるように装飾を付ける
		std::string consoleMessage = "[Log] " + message + "\n";
		OutputDebugStringA(consoleMessage.c_str());

		// 2. ファイルへの書き込み
		if (!gLogPath.empty())
		{
			// ios::app モードで開くことで、既存の内容を消さずに末尾に追記する
			std::ofstream file(gLogPath, std::ios::app);
			if (file.is_open())
			{
				file << message << std::endl;
			}
		}
	}
}