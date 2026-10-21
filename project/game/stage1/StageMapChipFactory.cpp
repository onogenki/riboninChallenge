#include "StageMapChipFactory.h"
#include "MapChipRegistry.h"

// CSVの並び順でP0とB0を処理し、生成物の所有とScene進行を分離します。
StageMapChipFactory::StageChipData StageMapChipFactory::BuildStageData(
	const MapChipField& field,
	const Vector3& origin,
	float cellSize,
	float floorY)
{
	StageChipData result;
	MapChipRegistry chipRegistry;
	chipRegistry.Register(MapChipType::PlayerStart, 0,
		[&result](const MapChipField::Chip& chip)
		{
			if (!result.playerStartChip) {
				result.playerStartChip = &chip;
			}
		});
	chipRegistry.Register(MapChipType::Block, 0,
		[&result, &field, &origin, cellSize, floorY](const MapChipField::Chip& chip)
		{
			result.blockData.push_back(CreateBlockData(field, chip, origin, cellSize, floorY));
		});
	for (const MapChipField::Chip& chip : field.GetChips()) {
		chipRegistry.Run(chip);
	}
	return result;
}

// Stage1のB0がどのモデル・座標・当たり判定になるかを一か所にそろえます。
LevelLoader::ObjectData StageMapChipFactory::CreateBlockData(
	const MapChipField& field,
	const MapChipField::Chip& chip,
	const Vector3& origin,
	float cellSize,
	float floorY)
{
	LevelLoader::ObjectData blockData{};
	blockData.type = "MESH";
	blockData.name =
		"MapChip_B0_" + std::to_string(chip.column) + "_" + std::to_string(chip.row);
	blockData.tag = "MapChip";
	blockData.objectType = "MAP_CHIP";
	blockData.fileName = "debug/block.obj";
	blockData.translation = field.GetPosition(chip, origin, cellSize);
	blockData.translation.y = floorY;
	blockData.scaling = { 1.0f, 1.0f, 1.0f };
	blockData.hasCollider = true;
	blockData.collider.type = "BOX";
	blockData.collider.size = { cellSize, cellSize, cellSize };
	return blockData;
}
