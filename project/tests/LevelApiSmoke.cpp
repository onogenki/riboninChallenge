#include "LevelLoader.h"
#include "MapChipField.h"
#include "MapChipRegistry.h"
#include "StageMapObjectIndex.h"
#include "StageMapChipFactory.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
	// JSONから読み込んだVector3の各成分を、軸入替も含めて比較します。
	bool MatchesVector3(const Vector3& actual, const Vector3& expected)
	{
		return actual.x == expected.x && actual.y == expected.y && actual.z == expected.z;
	}

	// Blender座標の照明を読み取った後、各Lightの値が期待通りか比較します。
	bool MatchesExpectedLighting(const LevelLoader::LightingData& light)
	{
		if (light.spotLights.size() != 1) {
			return false;
		}
		const LevelLoader::SpotLightData& spot = light.spotLights.front();
		return MatchesVector3(light.directionalColor, { 1.0f, 3.0f, 2.0f }) &&
			MatchesVector3(light.directionalDirection, { 4.0f, 6.0f, 5.0f }) &&
			light.directionalIntensity == 0.25f &&
			MatchesVector3(light.ambientColor, { 7.0f, 9.0f, 8.0f }) &&
			light.ambientIntensity == 0.125f &&
			MatchesVector3(light.pointColor, { 10.0f, 12.0f, 11.0f }) &&
			MatchesVector3(light.pointPosition, { 13.0f, 15.0f, 14.0f }) &&
			light.pointIntensity == 2.5f && light.pointRadius == 16.0f && light.pointDecay == 0.75f &&
			spot.name == "Key" &&
			MatchesVector3(spot.color, { 16.0f, 18.0f, 17.0f }) &&
			MatchesVector3(spot.position, { 19.0f, 21.0f, 20.0f }) &&
			spot.intensity == 3.0f &&
			MatchesVector3(spot.direction, { 22.0f, 24.0f, 23.0f }) &&
			spot.distance == 25.0f &&
			spot.decay == 0.5f && spot.cosAngle == 0.75f && spot.cosFalloffStart == 0.875f;
	}

	// 一時フォルダだけを使い、JSONの型検査・照明読込・Engine座標での保存を確認します。
	bool CheckLevelJsonValidationAndLighting()
	{
		namespace fs = std::filesystem;
		std::error_code error;
		const fs::path originalDirectory = fs::current_path(error);
		if (error) {
			return false;
		}
		const fs::path temporaryDirectory = fs::temp_directory_path(error) /
			("ribonin_level_smoke_" +
				std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		if (error || !fs::create_directory(temporaryDirectory, error)) {
			return false;
		}

		const fs::path resourcesDirectory = temporaryDirectory / "resources";
		const fs::path levelsDirectory = resourcesDirectory / "levels";
		const fs::path jsonPath = levelsDirectory / "Smoke.json";
		const fs::path temporaryJsonPath = levelsDirectory / "Smoke.json.tmp";
		bool passed = false;
		try {
			fs::create_directories(levelsDirectory);
			fs::current_path(temporaryDirectory);
			const auto loadMatches = [&](const char* jsonText, bool shouldLoad)
			{
				std::ofstream file(jsonPath, std::ios::trunc);
				file << jsonText;
				file.close();
				if (!file) {
					return false;
				}
				return static_cast<bool>(LevelLoader::Load("Smoke")) == shouldLoad;
			};

			passed = loadMatches(R"({"name":17,"objects":[]})", false) &&
				loadMatches(R"({"name":"scene","coordinate_system":17,"objects":[]})", false) &&
				loadMatches(R"({"name":"scene","coordinate_system":"engine","objects":[]})", true);
			// 照明を独立して読み、Blender座標系のY/Z変換と数値の保持を確認します。
			std::ofstream lightingFile(jsonPath, std::ios::trunc);
			lightingFile << R"({
				"name": "scene", "objects": [],
				"lighting": {
					"directional": {
						"color": [1, 2, 3], "direction": [4, 5, 6], "intensity": 0.25,
						"ambient": {"color": [7, 8, 9], "intensity": 0.125}
					},
					"point": {
						"color": [10, 11, 12], "position": [13, 14, 15],
						"intensity": 2.5, "radius": 16, "decay": 0.75
					},
					"spot_lights": [{
						"name": "Key", "color": [16, 17, 18], "position": [19, 20, 21],
						"intensity": 3, "direction": [22, 23, 24], "distance": 25,
						"decay": 0.5, "cos_angle": 0.75, "cos_falloff_start": 0.875
					}]
				}
			})";
			lightingFile.close();
			const std::unique_ptr<LevelLoader::LevelData> loaded = LevelLoader::Load("Smoke");
			if (!lightingFile || !loaded || !loaded->hasLighting || !MatchesExpectedLighting(loaded->lighting)) {
				passed = false;
			} else {
				// Stage1と同じEngine座標で保存し、全Lightの値を再読込でも保持します。
				LevelLoader::LevelData engineLevel;
				engineLevel.coordinateSystem = "engine";
				engineLevel.hasLighting = true;
				engineLevel.lighting = loaded->lighting;
				const bool saved = LevelLoader::Save("Smoke", engineLevel);
				const std::unique_ptr<LevelLoader::LevelData> reloaded = saved ? LevelLoader::Load("Smoke") : nullptr;
				passed = passed && saved && reloaded && reloaded->hasLighting &&
					MatchesExpectedLighting(reloaded->lighting);
				// Blender座標で読み込んだ照明も、保存・再読込後に同じ値へ戻る必要があります。
				const bool blenderSaved = LevelLoader::Save("Smoke", *loaded);
				const std::unique_ptr<LevelLoader::LevelData> blenderReloaded =
					blenderSaved ? LevelLoader::Load("Smoke") : nullptr;
				passed = passed && blenderSaved && blenderReloaded && blenderReloaded->hasLighting &&
					MatchesExpectedLighting(blenderReloaded->lighting);
			}
		} catch (...) {
			passed = false;
		}

		fs::current_path(originalDirectory, error);
		const bool restoredDirectory = !error;
		bool cleanedUp = true;
		// 作成した既知のパスだけを内側から消し、ほかの一時ファイルには触れません。
		for (const fs::path& path : { temporaryJsonPath, jsonPath, levelsDirectory, resourcesDirectory, temporaryDirectory }) {
			error.clear();
			fs::remove(path, error);
			cleanedUp = !error && cleanedUp;
		}
		return passed && restoredDirectory && cleanedUp;
	}
}

// CSVの番号別処理とレベルJSONの失敗契約を、Sceneを起動せずに確認します。
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
	check(CheckLevelJsonValidationAndLighting(),
		"invalid JSON is rejected; lighting survives Engine and Blender saves");
	std::cout << (failures == 0 ? "SUCCESS" : "FAILURE") << ": failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
