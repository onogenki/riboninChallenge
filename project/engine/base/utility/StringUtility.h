#pragma once
#include<string>


namespace StringUtility
{

	//stringをwstringに変換する
	std::wstring ConvertString(const std::string& str);

	std::string ConvertString(const std::wstring& str);

};

