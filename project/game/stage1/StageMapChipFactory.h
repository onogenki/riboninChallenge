#pragma once

#include "LevelLoader.h"
#include "MapChipField.h"
#include <vector>

// Stage1固有のマップチップを、既存のLevel配置物へ変換するFactoryです。
// CSVの解析はMapChipField、モデルの生成・所有はStageMapRuntimeが担当します。
class StageMapChipFactory
{
public:
	// CSVのP0とB0を分類した結果です。playerStartChipは元のfieldが所有します。
	struct StageChipData
	{
		const MapChipField::Chip* playerStartChip = nullptr;
		std::vector<LevelLoader::ObjectData> blockData;
	};

	// Stage1の対応済みChipだけを分類し、未登録の種類は従来どおり無視します。
	static StageChipData BuildStageData(
		const MapChipField& field,
		const Vector3& origin,
		float cellSize,
		float floorY);
	// B0一マスをblock.objと1x1x1のBOX Colliderへ変換します。
	static LevelLoader::ObjectData CreateBlockData(
		const MapChipField& field,
		const MapChipField::Chip& chip,
		const Vector3& origin,
		float cellSize,
		float floorY);
};
