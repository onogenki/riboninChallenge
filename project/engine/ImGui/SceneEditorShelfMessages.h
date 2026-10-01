#pragma once

#include <string>

// Model Shelfの追加・Preview結果を、Scene名と対象名を含む同じ形式の文章へ変換します。
namespace SceneEditorShelfMessages
{
	inline std::string MakeAddMessage(
		const std::string& sceneLabel,
		const char* kind,
		const std::string& fileName,
		bool success)
	{
		return std::string(success ? "Added " : "Could not add ") + sceneLabel + " " + kind + ": " + fileName;
	}

	inline std::string MakePreviewMessage(const char* kind, const std::string& fileName, bool success)
	{
		return std::string(success ? "Previewing " : "Could not preview ") + kind + ": " + fileName;
	}
}
