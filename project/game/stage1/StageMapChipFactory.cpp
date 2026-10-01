#include "StageMapChipFactory.h"

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
