#include "MapChipField.h"
#include "MapChipRegistry.h"
#include "StageMapObjectIndex.h"
#include "StageMapChipFactory.h"

#include <iostream>
#include <sstream>

// CSVを作らず、空白マス・番号別振分け・B0のCollider値を一緒に確認します。
int main()
{
	int failures = 0;
	const auto check = [&failures](bool condition, const char* name)
	{
		std::cout << (condition ? "PASS: " : "FAIL: ") << name << '\n';
		if (!condition) {
			++failures;
		}
	};

	std::istringstream csv("P0,,B0,E0,G0,C0,L0,X0,B1,B2\n,,B0");
	MapChipField field;
	check(field.LoadCsv(csv), "read in-memory CSV");
	check(field.GetChips().size() == 10, "blank cells create no chips");
	check(!field.LoadCsv("missing_map_chip_smoke.csv") && field.GetChips().size() == 10,
		"missing optional file does not discard loaded chips");

	int player = 0, enemy = 0, block0 = 0, block1 = 0;
	int gimmick = 0, checkpoint = 0, goal = 0;
	MapChipRegistry registry;
	check(!registry.Register(MapChipType::Unknown, 0, [](const auto&) {}),
		"unknown type cannot be registered");
	check(!registry.Register(MapChipType::Goal, 1, {}), "empty handler cannot be registered");
	registry.Register(MapChipType::PlayerStart, 0, [&](const auto&) { ++player; });
	registry.Register(MapChipType::EnemySpawn, 0, [&](const auto&) { ++enemy; });
	registry.Register(MapChipType::Block, 0, [&](const auto& chip)
		{
			++block0;
			const auto block = StageMapChipFactory::CreateBlockData(
				field, chip, { 22.0f, 0.0f, 33.0f }, 1.0f, -2.5f);
			check(block.fileName == "debug/block.obj" && block.hasCollider &&
				block.collider.type == "BOX" && block.collider.size.x == 1.0f &&
				block.collider.size.y == 1.0f && block.collider.size.z == 1.0f &&
				block.translation.x == 22.0f + chip.column &&
				block.translation.z == 33.0f + chip.row && block.translation.y == -2.5f,
				"B0 maps to a positioned 1x1x1 block and collider");
		});
	registry.Register(MapChipType::Block, 1, [&](const auto&) { ++block1; });
	registry.Register(MapChipType::Gimmick, 0, [&](const auto&) { ++gimmick; });
	registry.Register(MapChipType::Checkpoint, 0, [&](const auto&) { ++checkpoint; });
	registry.Register(MapChipType::Goal, 0, [&](const auto&) { ++goal; });
	int ignored = 0;
	for (const auto& chip : field.GetChips()) {
		if (!registry.Run(chip)) {
			++ignored;
		}
	}
	check(player == 1 && enemy == 1 && gimmick == 1 && checkpoint == 1 && goal == 1,
		"P0/E0/G0/C0/L0 use their own registered handlers");
	check(block0 == 2 && block1 == 1 && ignored == 2,
		"B0/B1 are separate; unknown X0 and unregistered B2 are ignored");

	// 編集UIが触るPlayerStartと、Sceneが読み取るPlayerStartが同じ元データか確認します。
	LevelLoader::LevelData levelData;
	LevelLoader::ObjectData firstStart;
	firstStart.tag = "PlayerStart";
	firstStart.translation = { 1.0f, 2.0f, 3.0f };
	LevelLoader::ObjectData parent;
	LevelLoader::ObjectData nestedStart;
	nestedStart.tag = "PlayerStart";
	nestedStart.translation = { 4.0f, 5.0f, 6.0f };
	parent.children.push_back(nestedStart);
	levelData.objects.push_back(firstStart);
	levelData.objects.push_back(parent);
	StageMapObjectIndex index;
	index.Build(levelData);
	auto* editableStart = StageMapObjectIndex::FindPlayerStartForEdit(levelData);
	check(editableStart && editableStart == index.GetPlayerStart(),
		"editor and stage select the same nested PlayerStart");
	if (editableStart) {
		editableStart->translation = { 7.0f, 8.0f, 9.0f };
	}
	check(index.GetPlayerStart() && index.GetPlayerStart()->translation.x == 7.0f,
		"editing PlayerStart updates the LevelData read by the stage");
	std::cout << (failures == 0 ? "SUCCESS" : "FAILURE") << ": failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
