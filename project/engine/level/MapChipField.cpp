#include "MapChipField.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>
#include <string_view>

namespace
{
	// CSVのセル前後にある空白と改行コードを取り除きます。
	std::string Trim(std::string value)
	{
		const auto first = std::find_if_not(
			value.begin(),
			value.end(),
			[](unsigned char character) { return std::isspace(character) != 0; });
		const auto last = std::find_if_not(
			value.rbegin(),
			value.rend(),
			[](unsigned char character) { return std::isspace(character) != 0; })
			.base();
		if (first >= last) {
			return {};
		}
		return { first, last };
	}

	// B0・P0の文字列を、種類と番号へ分解します。
	bool ParseChipCode(const std::string& code, char& type, uint32_t& subId)
	{
		if (code.empty() || !std::isalpha(static_cast<unsigned char>(code.front()))) {
			return false;
		}

		type = static_cast<char>(std::toupper(static_cast<unsigned char>(code.front())));
		subId = 0;
		if (code.size() == 1) {
			return true;
		}

		const char* firstNumber = code.data() + 1;
		const char* lastCharacter = code.data() + code.size();
		const std::from_chars_result result =
			std::from_chars(firstNumber, lastCharacter, subId);
		return result.ec == std::errc{} && result.ptr == lastCharacter;
	}
}

// ファイルを開き、セルの解析はストリーム版と共通の処理へ任せます。
bool MapChipField::LoadCsv(const std::string& filePath)
{
	std::ifstream file(filePath);
	return LoadCsv(file);
}

// 空白マスも列番号に数え、有効な記号だけを一時一覧へ読み取って最後に置き換えます。
bool MapChipField::LoadCsv(std::istream& stream)
{
	if (stream.fail()) {
		return false;
	}

	std::vector<Chip> loadedChips;
	std::string line;
	uint32_t row = 0;
	while (std::getline(stream, line)) {
		std::istringstream lineStream(line);
		std::string cell;
		uint32_t column = 0;
		while (std::getline(lineStream, cell, ',')) {
			const std::string code = Trim(std::move(cell));
			char type = ' ';
			uint32_t subId = 0;
			if (ParseChipCode(code, type, subId)) {
				loadedChips.push_back({ type, subId, column, row });
			}
			++column;
		}
		++row;
	}

	chips_ = std::move(loadedChips);
	return true;
}

MapChipType MapChipField::GetType(const Chip& chip)
{
	// 記号とゲーム上の役割の対応表です。
	// P0・P1の番号による違いは、使用するStage側でchip.subIdを見て決めます。
	switch (chip.type) {
	case 'P': return MapChipType::PlayerStart;
	case 'E': return MapChipType::EnemySpawn;
	case 'B': return MapChipType::Block;
	case 'G': return MapChipType::Gimmick;
	case 'C': return MapChipType::Checkpoint;
	case 'L': return MapChipType::Goal;
	default: return MapChipType::Unknown;
	}
}

const MapChipField::Chip* MapChipField::FindFirst(char type) const
{
	const char upperType = static_cast<char>(std::toupper(static_cast<unsigned char>(type)));
	const auto found = std::find_if(
		chips_.begin(),
		chips_.end(),
		[upperType](const Chip& chip)
		{
			return chip.type == upperType;
		});
	return found != chips_.end() ? &*found : nullptr;
}

const MapChipField::Chip* MapChipField::FindFirst(MapChipType type) const
{
	const auto found = std::find_if(
		chips_.begin(),
		chips_.end(),
		[type](const Chip& chip)
		{
			return GetType(chip) == type;
		});
	return found != chips_.end() ? &*found : nullptr;
}

Vector3 MapChipField::GetPosition(
	const Chip& chip,
	const Vector3& origin,
	float cellSize) const
{
	// CSVの右方向をゲームの+X、下方向をゲームの+Zとして配置します。
	return {
		origin.x + static_cast<float>(chip.column) * cellSize,
		origin.y,
		origin.z + static_cast<float>(chip.row) * cellSize,
	};
}
