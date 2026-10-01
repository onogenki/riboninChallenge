#pragma once
#include <string>
#include "DirectXCommon.h"

	namespace Logger
	{
		// ログファイルのパスを設定する
		void SetLogPath(const std::string& filePath);

		// ログを出力する
		void Log(const std::string& message);
	};
