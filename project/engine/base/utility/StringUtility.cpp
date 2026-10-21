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
		const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0);
		if (sizeNeeded <= 0) {
			return {};
		}

		//必要なサイズでwstringを確保
		std::wstring result(sizeNeeded, L'\0');

		// 変換失敗時に未完成の文字列を返さず、Audioなどの利用側へ空文字列を渡します。
		if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), result.data(), sizeNeeded) == 0) {
			return {};
		}

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
		const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0, nullptr, nullptr);
		if (sizeNeeded <= 0) {
			return {};
		}

		//必要なサイズでstringを確保
		std::string result(sizeNeeded, '\0');

		// 逆変換でも失敗を空文字列として返し、空バッファを参照しません。
		if (WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), result.data(), sizeNeeded, nullptr, nullptr) == 0) {
			return {};
		}

		return result;
	}
}
