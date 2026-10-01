#pragma once

#include "LevelLoader.h"
#include "MapChipField.h"

// Stage1固有のマップチップを、既存のLevel配置物へ変換するFactoryです。
// CSVの解析はMapChipField、モデルの生成・所有はStageMapRuntimeが担当します。
class StageMapChipFactory
{
public:
	// B0一マスをblock.objと1x1x1のBOX Colliderへ変換します。
	static LevelLoader::ObjectData CreateBlockData(
		const MapChipField& field,
		const MapChipField::Chip& chip,
		const Vector3& origin,
		float cellSize,
		float floorY);
};
