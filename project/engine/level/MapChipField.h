#pragma once

#include "Vector3.h"
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

// CSVの記号が表す、ゲーム上の配置物の種類です。
// 番号はChip::subIdに残すため、B0とB1のような種類ごとの違いも後から追加できます。
enum class MapChipType
{
	Unknown,
	PlayerStart,
	EnemySpawn,
	Block,
	Gimmick,
	Checkpoint,
	Goal,
};

// CSVに書いたマップチップを読み、マス番号と座標へ変換するLevel用クラスです。
// このクラスはモデルやPlayerを作らず、CSVのデータだけを保持します。
class MapChipField
{
public:
	// CSVの一マスから読み取った、種類・番号・マス番号です。
	struct Chip
	{
		// B0なら'B'、P0なら'P'です。
		char type = ' ';
		// B0なら0、B1なら1です。
		uint32_t subId = 0;
		// CSVの左から何列目かを表します。左端が0です。
		uint32_t column = 0;
		// CSVの上から何行目かを表します。上端が0です。
		uint32_t row = 0;
	};

	// CSVを読み、B0やP0などの有効なマップチップだけを保持します。
	bool LoadCsv(const std::string& filePath);
	// ファイルを作らず、同じ読込処理を文字列ストリームなどで確認するための入口です。
	bool LoadCsv(std::istream& stream);
	// 読み込んだ全マップチップを返します。
	const std::vector<Chip>& GetChips() const { return chips_; }
	// P・E・Bなどの記号を、共通のマップチップ種類へ変換します。
	static MapChipType GetType(const Chip& chip);
	// 指定種類で最初に見つかったマップチップを返します。見つからなければnullptrです。
	const Chip* FindFirst(char type) const;
	// 指定した共通種類で最初に見つかったマップチップを返します。見つからなければnullptrです。
	const Chip* FindFirst(MapChipType type) const;
	// CSVの列・行番号を、ゲーム内の3D座標へ変換します。
	Vector3 GetPosition(
		const Chip& chip,
		const Vector3& origin,
		float cellSize) const;

private:
	// CSVから読み取った、空白ではないマップチップです。
	std::vector<Chip> chips_;
};
