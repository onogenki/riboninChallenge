#include "LevelLoader.h"
#include "MapChipField.h"
#include "MapChipRegistry.h"
#include "StageMapObjectIndex.h"
#include "StageMapChipFactory.h"
#include "StageMapFileMonitor.h"
#include "StageMapSelection.h"
#include "StageMapRepository.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <Windows.h>

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

	// Stage番号と外部ファイルの対応、および失敗時に選択・予約を壊さない契約を確認します。
	bool CheckStageMapSelection()
	{
		StageMapSelection selection(1);
		if (selection.GetStageId() != 1 || selection.GetDefinition() ||
			selection.TakeRequestedStageId() != 0) {
			return false;
		}

		constexpr const char* kExpectedNames[] = {
			"stage1", "stage2", "stage3", "stage4", "stage5"
		};
		for (int stageId = 1; stageId <= 5; ++stageId) {
			if (!selection.Select(stageId)) {
				return false;
			}
			const StageMapSelection::Definition* definition = selection.GetDefinition();
			const std::string name = kExpectedNames[stageId - 1];
			if (!definition || selection.GetStageId() != stageId || definition->id != stageId ||
				definition->levelFileName != name ||
				definition->levelFilePath != "resources/levels/" + name + ".json" ||
				definition->mapChipFilePath != "resources/maps/" + name + ".csv") {
				return false;
			}
		}

		const StageMapSelection::Definition* activeDefinition = selection.GetDefinition();
		if (selection.Select(0) || selection.Select(6) ||
			selection.GetStageId() != 5 || selection.GetDefinition() != activeDefinition) {
			return false;
		}
		if (!selection.Request(2) || selection.Request(6) ||
			selection.TakeRequestedStageId() != 2 || selection.TakeRequestedStageId() != 0) {
			return false;
		}
		return selection.GetStageId() == 5 && selection.GetDefinition() == activeDefinition;
	}

	// Stage IDごとに別のJSONを読み、右Inspectorへ渡す一覧に前Stageの配置物が混ざらないことを確認します。
	bool CheckStageJsonIsolation()
	{
		StageMapRepository repository;
		std::string status;
		for (int stageId = 1; stageId <= 5; ++stageId) {
			if (!repository.SelectAndReload(stageId, []() { return true; }, status) ||
				repository.GetStageId() != stageId || !repository.GetLevelData()) {
				return false;
			}
			const LevelLoader::LevelData& levelData = *repository.GetLevelData();
			StageMapObjectIndex index;
			index.Build(levelData);
			if (!index.GetFloor() || !index.GetPlayerStart()) {
				return false;
			}
			if (stageId == 1) {
				if (levelData.objects.size() <= 2) {
					return false;
				}
				continue;
			}
			const std::string expectedFloorName = "Stage" + std::to_string(stageId) + "Floor";
			if (levelData.objects.size() != 2 || index.GetFloor()->name != expectedFloorName ||
				!index.GetFixedMirrors().empty() || !index.GetRuntimeObjects().empty()) {
				return false;
			}
		}
		return true;
	}

	// Inspectorで改名・BOX追加した通常モデルを、Stage JSONへ保存して再読込しても保てるか確認します。
	bool CheckModelMetadataJsonRoundTrip()
	{
		const std::string fileName = "SmokeMetadata_" +
			std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		const std::filesystem::path jsonPath =
			std::filesystem::path("resources/levels") / (fileName + ".json");
		std::filesystem::path temporaryPath = jsonPath;
		temporaryPath += ".tmp";
		LevelLoader::LevelData levelData{};
		levelData.coordinateSystem = "engine";
		LevelLoader::ObjectData model{};
		model.type = "MESH";
		model.name = "HallFloor";
		model.tag = "EditorAdded";
		model.objectType = "STATIC";
		model.fileName = "debug/floor.obj";
		model.hasCollider = true;
		model.collider.type = "BOX";
		model.collider.center = { 1.0f, 0.0f, -2.0f };
		model.collider.size = { 4.0f, 0.5f, 6.0f };
		levelData.objects.push_back(std::move(model));
		const bool saved = LevelLoader::Save(fileName, levelData);
		const std::unique_ptr<LevelLoader::LevelData> loaded =
			saved ? LevelLoader::Load(fileName) : nullptr;
		bool passed = loaded && loaded->objects.size() == 1;
		if (passed) {
			const LevelLoader::ObjectData& reloaded = loaded->objects.front();
			passed = reloaded.name == "HallFloor" && reloaded.fileName == "debug/floor.obj" &&
				reloaded.tag == "EditorAdded" && reloaded.hasCollider &&
				reloaded.collider.type == "BOX" &&
				MatchesVector3(reloaded.collider.center, { 1.0f, 0.0f, -2.0f }) &&
				MatchesVector3(reloaded.collider.size, { 4.0f, 0.5f, 6.0f });
		}
		std::error_code cleanupError;
		std::filesystem::remove(jsonPath, cleanupError);
		std::filesystem::remove(temporaryPath, cleanupError);
		return passed && !cleanupError;
	}

	// 専用の一時JSON/CSVだけを使い、変更検知とゲーム内保存後の時刻同期を確認します。
	bool CheckStageMapFileMonitor()
	{
		namespace fs = std::filesystem;
		std::error_code error;
		const fs::path temporaryDirectory = fs::current_path(error) /
			("ribonin_map_monitor_smoke_" +
				std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		if (error || !fs::create_directory(temporaryDirectory, error)) {
			return false;
		}
		const fs::path jsonPath = temporaryDirectory / "map.json";
		const fs::path csvPath = temporaryDirectory / "map.csv";
		bool passed = false;
		try {
			{
				std::ofstream jsonFile(jsonPath);
				std::ofstream csvFile(csvPath);
				jsonFile << "{}";
				csvFile << "B0";
			}
			StageMapFileMonitor monitor;
			monitor.Start(jsonPath.string(), csvPath.string());
			passed = monitor.GetJsonFilePath() == jsonPath.string() && !monitor.ConsumeChange();
			const fs::file_time_type jsonTime = fs::last_write_time(jsonPath);
			fs::last_write_time(jsonPath, jsonTime + std::chrono::seconds(10));
			passed = monitor.ConsumeChange() && !monitor.ConsumeChange() && passed;
			const fs::file_time_type csvTime = fs::last_write_time(csvPath);
			fs::last_write_time(csvPath, csvTime + std::chrono::seconds(10));
			passed = monitor.ConsumeChange() && !monitor.ConsumeChange() && passed;
			fs::last_write_time(jsonPath, jsonTime + std::chrono::seconds(20));
			monitor.SynchronizeJson();
			passed = !monitor.ConsumeChange() && passed;
			// 両方の保存を同じフレームで見つけても、次フレームにCSVだけを持ち越さないことを確認します。
			fs::last_write_time(jsonPath, jsonTime + std::chrono::seconds(30));
			fs::last_write_time(csvPath, csvTime + std::chrono::seconds(20));
			passed = monitor.ConsumeChange() && !monitor.ConsumeChange() && passed;
		} catch (...) {
			std::cerr << "DETAIL FAIL: map monitor test threw an exception\n";
			passed = false;
		}
		// 作成した既知の一時ファイルとフォルダだけを消します。
		bool cleanedUp = true;
		for (const fs::path& path : { jsonPath, csvPath, temporaryDirectory }) {
			error.clear();
			fs::remove(path, error);
			cleanedUp = !error && cleanedUp;
		}
		return passed && cleanedUp;
	}

	// 作業領域内の固有フォルダだけを使い、JSONの型検査・照明読込・保存を確認します。
	bool CheckLevelJsonValidationAndLighting()
	{
		namespace fs = std::filesystem;
		std::error_code error;
		const fs::path originalDirectory = fs::current_path(error);
		if (error) {
			return false;
		}
		// Windowsの一時フォルダでは置換が拒否される環境もあるため、実際のStageと同じ作業領域で確認します。
		const fs::path temporaryDirectory = originalDirectory /
			("ribonin_level_smoke_" +
				std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		if (error || !fs::create_directory(temporaryDirectory, error)) {
			return false;
		}

		const fs::path resourcesDirectory = temporaryDirectory / "resources";
		const fs::path levelsDirectory = resourcesDirectory / "levels";
		const fs::path jsonPath = levelsDirectory / "Smoke.json";
		const fs::path temporaryJsonPath = levelsDirectory / "Smoke.json.tmp";
		const fs::path newJsonPath = levelsDirectory / "SmokeNew.json";
		const fs::path newTemporaryJsonPath = levelsDirectory / "SmokeNew.json.tmp";
		const fs::path stageJsonPath = levelsDirectory / "stage1.json";
		const fs::path stageTemporaryJsonPath = levelsDirectory / "stage1.json.tmp";
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
			// Stage専用の仮JSONをロックし、保存失敗時の座標系と成功後の保存形式を確認します。
			std::ofstream stageFile(stageJsonPath, std::ios::trunc);
			stageFile << R"({"name":"scene","coordinate_system":"blender","objects":[]})";
			stageFile.close();
			StageMapRepository repository;
			std::string repositoryStatus;
			const bool stageLoaded = stageFile && repository.SelectAndReload(1, []() { return true; }, repositoryStatus);
			const HANDLE lockedStage = CreateFileW(stageJsonPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
				nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			bool repositorySaveChecks = false;
			if (stageLoaded && lockedStage != INVALID_HANDLE_VALUE) {
				const bool rejected = !repository.Save(repositoryStatus);
				const bool keptCoordinates = repository.GetLevelData()->coordinateSystem == "blender";
				CloseHandle(lockedStage);
				const bool saved = repository.Save(repositoryStatus);
				const auto savedStage = LevelLoader::Load("stage1");
				repositorySaveChecks = rejected && keptCoordinates && saved && savedStage &&
					repository.GetLevelData()->coordinateSystem == "engine" && savedStage->coordinateSystem == "engine";
				// 不正なUTF-8でJSON出力を例外終了させ、保存済みJSONと編集中の座標系を守れるか確認します。
				repository.GetLevelData()->coordinateSystem = "blender";
				LevelLoader::ObjectData invalidObject;
				invalidObject.name = std::string(1, static_cast<char>(0xff));
				repository.GetLevelData()->objects.push_back(invalidObject);
				bool exceptionThrown = false;
				try {
					repository.Save(repositoryStatus);
				} catch (const std::exception&) {
					exceptionThrown = true;
				}
				const auto afterException = LevelLoader::Load("stage1");
				repositorySaveChecks = repositorySaveChecks && exceptionThrown &&
					repository.GetLevelData()->coordinateSystem == "blender" &&
					afterException && afterException->coordinateSystem == "engine" && afterException->objects.empty();
			} else if (lockedStage != INVALID_HANDLE_VALUE) {
				CloseHandle(lockedStage);
			}
			if (!repositorySaveChecks) {
				std::cerr << "DETAIL FAIL: Stage save coordinate rollback\n";
			}
			passed = passed && repositorySaveChecks;
			if (!passed) {
				std::cerr << "DETAIL FAIL: JSON type validation\n";
			}
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
				std::cerr << "DETAIL FAIL: Blender lighting load\n";
				passed = false;
			} else {
				// Stage1と同じEngine座標で保存し、全Lightの値を再読込でも保持します。
				LevelLoader::LevelData engineLevel;
				engineLevel.coordinateSystem = "engine";
				engineLevel.hasLighting = true;
				engineLevel.lighting = loaded->lighting;
				const bool saved = LevelLoader::Save("Smoke", engineLevel);
				const DWORD engineSaveError = saved ? ERROR_SUCCESS : GetLastError();
				const std::unique_ptr<LevelLoader::LevelData> reloaded = saved ? LevelLoader::Load("Smoke") : nullptr;
				const bool engineRoundTrip = saved && reloaded && reloaded->hasLighting &&
					MatchesExpectedLighting(reloaded->lighting);
				if (!engineRoundTrip) {
					std::cerr << "DETAIL FAIL: Engine lighting save/reload saved=" << saved
						<< " error=" << engineSaveError << " loaded=" << static_cast<bool>(reloaded) << '\n';
				}
				passed = passed && engineRoundTrip;
				// Blender座標で読み込んだ照明も、保存・再読込後に同じ値へ戻る必要があります。
				const bool blenderSaved = LevelLoader::Save("Smoke", *loaded);
				const DWORD blenderSaveError = blenderSaved ? ERROR_SUCCESS : GetLastError();
				const std::unique_ptr<LevelLoader::LevelData> blenderReloaded =
					blenderSaved ? LevelLoader::Load("Smoke") : nullptr;
				const bool blenderRoundTrip = blenderSaved && blenderReloaded && blenderReloaded->hasLighting &&
					MatchesExpectedLighting(blenderReloaded->lighting);
				if (!blenderRoundTrip) {
					std::cerr << "DETAIL FAIL: Blender lighting save/reload saved=" << blenderSaved
						<< " error=" << blenderSaveError << " loaded=" << static_cast<bool>(blenderReloaded) << '\n';
				}
				passed = passed && blenderRoundTrip;
				// 既存ファイルがない時も、完成した一時ファイルを新しい名前へ移せます。
				const bool newFileSaved = LevelLoader::Save("SmokeNew", engineLevel);
				const DWORD newFileSaveError = newFileSaved ? ERROR_SUCCESS : GetLastError();
				const std::unique_ptr<LevelLoader::LevelData> newFileLoaded =
					newFileSaved ? LevelLoader::Load("SmokeNew") : nullptr;
				const bool newFileRoundTrip = newFileSaved && newFileLoaded && newFileLoaded->hasLighting &&
					MatchesExpectedLighting(newFileLoaded->lighting) && !fs::exists(newTemporaryJsonPath);
				if (!newFileRoundTrip) {
					std::cerr << "DETAIL FAIL: new JSON save/reload saved=" << newFileSaved
						<< " error=" << newFileSaveError << " loaded=" << static_cast<bool>(newFileLoaded) << '\n';
				}
				passed = passed && newFileRoundTrip;
				// 削除共有を許さず既存JSONを開き、置換失敗でも元ファイルを読めるか調べます。
				const HANDLE lockedFile = CreateFileW(
					jsonPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
					nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
				if (lockedFile == INVALID_HANDLE_VALUE) {
					std::cerr << "DETAIL FAIL: open locked JSON\n";
					passed = false;
				} else {
					LevelLoader::LevelData changedLevel{};
					changedLevel.coordinateSystem = "engine";
					const bool replacedWhileLocked = LevelLoader::Save("Smoke", changedLevel);
					CloseHandle(lockedFile);
					const std::unique_ptr<LevelLoader::LevelData> keptFile = LevelLoader::Load("Smoke");
					const bool lockedFilePreserved = !replacedWhileLocked && keptFile && keptFile->hasLighting &&
						MatchesExpectedLighting(keptFile->lighting) && fs::exists(temporaryJsonPath);
					if (!lockedFilePreserved) {
						std::cerr << "DETAIL FAIL: locked JSON replacement preserves original\n";
					}
					passed = passed && lockedFilePreserved;
				}
			}
		} catch (...) {
			std::cerr << "DETAIL FAIL: JSON test threw an exception\n";
			passed = false;
		}

		fs::current_path(originalDirectory, error);
		const bool restoredDirectory = !error;
		bool cleanedUp = true;
		// 作成した既知のパスだけを内側から消し、ほかの一時ファイルには触れません。
		for (const fs::path& path : {
			stageTemporaryJsonPath, stageJsonPath, newTemporaryJsonPath, newJsonPath, temporaryJsonPath, jsonPath,
			levelsDirectory, resourcesDirectory, temporaryDirectory }) {
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

	// Stage本編と同じFactory経路で、最初のP0と二つのB0だけが採用されることを確認します。
	const StageMapChipFactory::StageChipData stageChipData = StageMapChipFactory::BuildStageData(
		field, { 22.0f, 0.0f, 33.0f }, 1.0f, -2.5f);
	check(!field.GetChips().empty() &&
		stageChipData.playerStartChip == &field.GetChips().front() &&
		stageChipData.blockData.size() == 2 &&
		MatchesVector3(stageChipData.blockData[0].translation, { 24.0f, -2.5f, 33.0f }) &&
		MatchesVector3(stageChipData.blockData[1].translation, { 24.0f, -2.5f, 34.0f }) &&
		stageChipData.blockData[0].hasCollider && stageChipData.blockData[1].hasCollider,
		"Stage factory keeps first P0 and ordered B0 blocks only");

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
		"JSON validation, new/replacement saves, and failed replacement preserve the old file");
	check(CheckStageMapSelection(),
		"Stage IDs 1-5 map to files; invalid IDs preserve selection and requests run once");
	check(CheckStageJsonIsolation(),
		"Stage 1-5 load separate JSON objects without carrying Stage1 models into the next Inspector");
	check(CheckModelMetadataJsonRoundTrip(),
		"renamed model and BOX collider survive JSON save and reload");
	check(CheckStageMapFileMonitor(),
		"JSON and CSV changes reload once, including simultaneous saves; saving JSON synchronizes its timestamp");
	std::cout << (failures == 0 ? "SUCCESS" : "FAILURE") << ": failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
