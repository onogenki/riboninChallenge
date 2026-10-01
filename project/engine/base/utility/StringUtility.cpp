#include "StringUtility.h"
#include <Windows.h>

namespace StringUtility
{
	//std::string(マルチバイト)->std::wstring(ワイド文字)
	std::wstring ConvertString(const std::string& str)
	{
		if (str.empty())
		{
			return std::wstring();
		}
		//変換後に必要となるバッファのサイズを取得
		int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0);

		//必要なサイズでwstringを確保
		std::wstring result(sizeNeeded, L'\0');

		//実際に変換を行う
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), &result[0], sizeNeeded);

		return result;
	}

	//std::string(ワイド文字)->std::string(マルチバイト)
	std::string ConvertString(const std::wstring& str)
	{
		if (str.empty())
		{
			return std::string();
		}
		//変換後に必要となるバッファのサイズ(バイト数)を取得
		int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0, nullptr, nullptr);

		//必要なサイズでstringを確保
		std::string result(sizeNeeded, '\0');

		//実際に変換を行う
		WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), &result[0], sizeNeeded, nullptr, nullptr);

		return result;
	}
}
