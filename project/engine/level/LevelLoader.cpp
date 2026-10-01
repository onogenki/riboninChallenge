#include "LevelLoader.h"
#include <fstream>
#include <cassert>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <numbers>
#include "../../externals/nlohmann/json.hpp"

namespace
{
	// JSONの再帰変換はLevelLoader.cppだけで使い、LevelLoader.hへJSON型を公開しません。
	LevelLoader::ObjectData LoadObject(
		const nlohmann::json& object,
		bool usesEngineCoordinates = false);
	void LoadObjects(
		const nlohmann::json& objects,
		std::vector<LevelLoader::ObjectData>& objectList,
		bool usesEngineCoordinates = false);

	nlohmann::json SaveVector3(const Vector3& value)
	{
		return nlohmann::json::array({ value.x, value.y, value.z });
	}

	Vector3 LoadVector3(const nlohmann::json& value, bool usesEngineCoordinates)
	{
		if (usesEngineCoordinates) {
			return {
				value[0].get<float>(),
				value[1].get<float>(),
				value[2].get<float>(),
			};
		}
		return {
			value[0].get<float>(),
			value[2].get<float>(),
			value[1].get<float>(),
		};
	}

	nlohmann::json SaveObjectData(
		const LevelLoader::ObjectData& objectData,
		bool usesEngineCoordinates)
	{
		constexpr float kRadianToDegree = 180.0f / std::numbers::pi_v<float>;
		nlohmann::json object;
		object["type"] = objectData.type;
		object["name"] = objectData.name;
		if (!objectData.tag.empty()) {
			object["tag"] = objectData.tag;
		}
		if (!objectData.objectType.empty()) {
			object["object_type"] = objectData.objectType;
		}
		if (!objectData.fileName.empty()) {
			object["file_name"] = objectData.fileName;
		}
		if (!objectData.eventId.empty()) {
			object["event_id"] = objectData.eventId;
		}
		if (!objectData.eventCameraName.empty()) {
			object["event_camera"] = objectData.eventCameraName;
		}
		if (objectData.hasCameraFocus) {
			if (usesEngineCoordinates) {
				object["camera_focus"] = SaveVector3(objectData.cameraFocus);
			} else {
				object["camera_focus"] = SaveVector3({
					objectData.cameraFocus.x,
					objectData.cameraFocus.z,
					objectData.cameraFocus.y,
				});
			}
		}
		if (objectData.hasCameraArea) {
			object["camera_area"] = {
				{ "distance", objectData.cameraArea.distance },
				{ "pitch", objectData.cameraArea.pitch },
				{ "fov_y", objectData.cameraArea.fovY },
			};
		}
		if (!objectData.controlPoints.empty()) {
			object["control_points"] = nlohmann::json::array();
			for (const Vector3& controlPoint : objectData.controlPoints) {
				if (usesEngineCoordinates) {
					object["control_points"].push_back(SaveVector3(controlPoint));
				} else {
					object["control_points"].push_back(SaveVector3({
						controlPoint.x,
						controlPoint.z,
						controlPoint.y,
					}));
				}
			}
			object["path_speed"] = objectData.pathSpeed;
			object["path_loop"] = objectData.pathLoop;
		}

		nlohmann::json transform;
		if (usesEngineCoordinates) {
			transform["translation"] = SaveVector3(objectData.translation);
			transform["rotation"] = SaveVector3({
				objectData.rotation.x * kRadianToDegree,
				objectData.rotation.y * kRadianToDegree,
				objectData.rotation.z * kRadianToDegree,
			});
			transform["scaling"] = SaveVector3(objectData.scaling);
		} else {
			transform["translation"] = SaveVector3({
				objectData.translation.x,
				objectData.translation.z,
				objectData.translation.y,
			});
			transform["rotation"] = SaveVector3({
				-objectData.rotation.x * kRadianToDegree,
				-objectData.rotation.z * kRadianToDegree,
				-objectData.rotation.y * kRadianToDegree,
			});
			transform["scaling"] = SaveVector3({
				objectData.scaling.x,
				objectData.scaling.z,
				objectData.scaling.y,
			});
		}
		object["transform"] = std::move(transform);

		if (objectData.hasCollider) {
			nlohmann::json collider;
			collider["type"] = objectData.collider.type;
			if (usesEngineCoordinates) {
				collider["center"] = SaveVector3(objectData.collider.center);
				collider["size"] = SaveVector3(objectData.collider.size);
			} else {
				collider["center"] = SaveVector3({
					objectData.collider.center.x,
					objectData.collider.center.z,
					objectData.collider.center.y,
				});
				collider["size"] = SaveVector3({
					objectData.collider.size.x,
					objectData.collider.size.z,
					objectData.collider.size.y,
				});
			}
			object["collider"] = std::move(collider);
		}

		if (!objectData.children.empty()) {
			object["children"] = nlohmann::json::array();
			for (const LevelLoader::ObjectData& child : objectData.children) {
				object["children"].push_back(SaveObjectData(child, usesEngineCoordinates));
			}
		}
		return object;
	}
}

std::unique_ptr<LevelLoader::LevelData> LevelLoader::Load(const std::string& fileName)
{
	const std::string fullpath = "resources/levels/" + fileName + ".json";

	//ファイルを開く
	std::ifstream file(fullpath);

	//ファイルオープン失敗をチェック
	if (file.fail()) {
		return nullptr;
	}

	//JSON文字列から解凍したデータ
	nlohmann::json deserialized;

	//解凍
	try {
		file >> deserialized;
	} catch (const nlohmann::json::exception&) {
		return nullptr;
	}

	if (!deserialized.is_object() || !deserialized.contains("name") ||
		deserialized.value("name", "") != "scene" ||
		!deserialized.contains("objects") || !deserialized["objects"].is_array()) {
		return nullptr;
	}

	//正しいレベルエディタファイルかチェック
	assert(deserialized.is_object());
	assert(deserialized.contains("name"));
	assert(deserialized["name"].get<std::string>() == "scene");
	assert(deserialized.contains("objects"));

	//レベルエディタ格納インスタンスを生成
	auto levelData = std::make_unique<LevelData>();
	levelData->coordinateSystem = deserialized.value("coordinate_system", "blender");
	const bool usesEngineCoordinates = levelData->coordinateSystem == "engine";

	try {
		if (deserialized.contains("stage_start")) {
			const nlohmann::json& stageStart = deserialized["stage_start"];
			levelData->hasStageStart = true;
			levelData->stageStart.duration = stageStart.value("duration", 2.5f);
			levelData->stageStart.playerAirHeight = stageStart.value("player_air_height", 8.0f);
			levelData->stageStart.cameraFrontDistance = stageStart.value("camera_front_distance", 10.0f);
			levelData->stageStart.cameraFrontHeight = stageStart.value("camera_front_height", 6.0f);
			levelData->stageStart.cameraOrbitAngle = stageStart.value("camera_orbit_angle", 3.14159265f);
			levelData->stageStart.cameraHandoffDuration = stageStart.value("camera_handoff_duration", 0.8f);
		}
		if (deserialized.contains("lighting")) {
			const nlohmann::json& lighting = deserialized["lighting"];
			levelData->hasLighting = true;
			if (lighting.contains("directional")) {
				const nlohmann::json& directional = lighting["directional"];
				levelData->lighting.directionalColor = LoadVector3(directional["color"], usesEngineCoordinates);
				levelData->lighting.directionalDirection = LoadVector3(directional["direction"], usesEngineCoordinates);
				levelData->lighting.directionalIntensity = directional.value("intensity", 0.3f);
				if (directional.contains("ambient")) {
					const nlohmann::json& ambient = directional["ambient"];
					levelData->lighting.ambientColor = LoadVector3(ambient["color"], usesEngineCoordinates);
					levelData->lighting.ambientIntensity = ambient.value("intensity", 0.0f);
				}
			}
			if (lighting.contains("point")) {
				const nlohmann::json& point = lighting["point"];
				levelData->lighting.pointColor = LoadVector3(point["color"], usesEngineCoordinates);
				levelData->lighting.pointPosition = LoadVector3(point["position"], usesEngineCoordinates);
				levelData->lighting.pointIntensity = point.value("intensity", 5.0f);
				levelData->lighting.pointRadius = point.value("radius", 20.0f);
				levelData->lighting.pointDecay = point.value("decay", 1.0f);
			}
			if (lighting.contains("spot_lights")) {
				for (const nlohmann::json& spotLight : lighting["spot_lights"]) {
					SpotLightData spotLightData{};
					spotLightData.name = spotLight.value("name", "SpotLight");
					spotLightData.color = LoadVector3(spotLight["color"], usesEngineCoordinates);
					spotLightData.position = LoadVector3(spotLight["position"], usesEngineCoordinates);
					spotLightData.intensity = spotLight.value("intensity", 0.0f);
					spotLightData.direction = LoadVector3(spotLight["direction"], usesEngineCoordinates);
					spotLightData.distance = spotLight.value("distance", 0.0f);
					spotLightData.decay = spotLight.value("decay", 1.0f);
					spotLightData.cosAngle = spotLight.value("cos_angle", 0.5f);
					spotLightData.cosFalloffStart = spotLight.value("cos_falloff_start", 0.8f);
					levelData->lighting.spotLights.push_back(std::move(spotLightData));
				}
			}
		}
		LoadObjects(deserialized["objects"], levelData->objects, usesEngineCoordinates);
	} catch (const nlohmann::json::exception&) {
		return nullptr;
	}

	return levelData;
}

bool LevelLoader::Save(const std::string& fileName, const LevelData& levelData)
{
	const std::string fullPath = "resources/levels/" + fileName + ".json";
	const std::string temporaryPath = fullPath + ".tmp";
	const bool usesEngineCoordinates = levelData.coordinateSystem == "engine";

	nlohmann::json serialized;
	serialized["name"] = "scene";
	serialized["coordinate_system"] = levelData.coordinateSystem;
	if (levelData.hasStageStart) {
		serialized["stage_start"] = {
			{ "duration", levelData.stageStart.duration },
			{ "player_air_height", levelData.stageStart.playerAirHeight },
			{ "camera_front_distance", levelData.stageStart.cameraFrontDistance },
			{ "camera_front_height", levelData.stageStart.cameraFrontHeight },
			{ "camera_orbit_angle", levelData.stageStart.cameraOrbitAngle },
			{ "camera_handoff_duration", levelData.stageStart.cameraHandoffDuration },
		};
	}
	if (levelData.hasLighting) {
		nlohmann::json lighting;
		lighting["directional"] = {
			{ "color", SaveVector3(levelData.lighting.directionalColor) },
			{ "direction", SaveVector3(levelData.lighting.directionalDirection) },
			{ "intensity", levelData.lighting.directionalIntensity },
			{ "ambient", {
				{ "color", SaveVector3(levelData.lighting.ambientColor) },
				{ "intensity", levelData.lighting.ambientIntensity },
			} },
		};
		lighting["point"] = {
			{ "color", SaveVector3(levelData.lighting.pointColor) },
			{ "position", SaveVector3(levelData.lighting.pointPosition) },
			{ "intensity", levelData.lighting.pointIntensity },
			{ "radius", levelData.lighting.pointRadius },
			{ "decay", levelData.lighting.pointDecay },
		};
		lighting["spot_lights"] = nlohmann::json::array();
		for (const SpotLightData& spotLight : levelData.lighting.spotLights) {
			lighting["spot_lights"].push_back({
				{ "name", spotLight.name },
				{ "color", SaveVector3(spotLight.color) },
				{ "position", SaveVector3(spotLight.position) },
				{ "intensity", spotLight.intensity },
				{ "direction", SaveVector3(spotLight.direction) },
				{ "distance", spotLight.distance },
				{ "decay", spotLight.decay },
				{ "cos_angle", spotLight.cosAngle },
				{ "cos_falloff_start", spotLight.cosFalloffStart },
			});
		}
		serialized["lighting"] = std::move(lighting);
	}
	serialized["objects"] = nlohmann::json::array();
	for (const ObjectData& objectData : levelData.objects) {
		serialized["objects"].push_back(SaveObjectData(objectData, usesEngineCoordinates));
	}

	// まず一時ファイルを完成させ、書込途中のJSONをゲームが読まないようにする。
	{
		std::ofstream file(temporaryPath, std::ios::trunc);
		if (file.fail()) {
			return false;
		}
		file << std::setw(4) << serialized << '\n';
		if (file.fail()) {
			return false;
		}
	}

	std::error_code error;
	std::filesystem::copy_file(
		temporaryPath,
		fullPath,
		std::filesystem::copy_options::overwrite_existing,
		error);
	std::filesystem::remove(temporaryPath);
	return !error;
}

namespace
{
	// JSONをObjectDataへ変換する詳細処理は、この実装ファイル内だけに閉じます。
	LevelLoader::ObjectData LoadObject(const nlohmann::json& object, bool usesEngineCoordinates)
{
	LevelLoader::ObjectData objectData{};
	objectData.type = object.at("type").get<std::string>();

	if (object.contains("name"))
	{
		objectData.name = object["name"].get<std::string>();
	}

	if (object.contains("tag"))
	{
		objectData.tag = object["tag"].get<std::string>();
	}

	if (object.contains("object_type"))
	{
		objectData.objectType = object["object_type"].get<std::string>();
	}

	if (object.contains("file_name"))
	{
		objectData.fileName = object["file_name"].get<std::string>();
	}

	if (object.contains("event_id"))
	{
		objectData.eventId = object["event_id"].get<std::string>();
	}

	if (object.contains("event_camera"))
	{
		objectData.eventCameraName = object["event_camera"].get<std::string>();
	}

	if (object.contains("camera_focus"))
	{
		const nlohmann::json& focus = object["camera_focus"];
		objectData.hasCameraFocus = true;
		if (usesEngineCoordinates) {
			objectData.cameraFocus = {
				focus[0].get<float>(),
				focus[1].get<float>(),
				focus[2].get<float>(),
			};
		} else {
			objectData.cameraFocus = {
				focus[0].get<float>(),
				focus[2].get<float>(),
				focus[1].get<float>(),
			};
		}
	}

	if (object.contains("camera_area"))
	{
		const nlohmann::json& cameraArea = object["camera_area"];
		objectData.hasCameraArea = true;
		objectData.cameraArea.distance = cameraArea.value("distance", 10.77f);
		objectData.cameraArea.pitch = cameraArea.value("pitch", 0.38050638f);
		objectData.cameraArea.fovY = cameraArea.value("fov_y", 0.45f);
	}

	if (object.contains("control_points"))
	{
		const nlohmann::json& controlPoints = object["control_points"];
		for (const nlohmann::json& point : controlPoints) {
			if (usesEngineCoordinates) {
				objectData.controlPoints.push_back({
					point[0].get<float>(),
					point[1].get<float>(),
					point[2].get<float>(),
				});
			} else {
				objectData.controlPoints.push_back({
					point[0].get<float>(),
					point[2].get<float>(),
					point[1].get<float>(),
				});
			}
		}
		objectData.pathSpeed = object.value("path_speed", 1.0f);
		objectData.pathLoop = object.value("path_loop", true);
	}

	if (object.contains("transform"))
	{
		const nlohmann::json& transform = object["transform"];
		constexpr float kDegreeToRadian = std::numbers::pi_v<float> / 180.0f;

		if (usesEngineCoordinates) {
			// ゲーム内編集用JSONは、画面で使うX・Y・Zの順番をそのまま読む。
			objectData.translation.x = transform["translation"][0].get<float>();
			objectData.translation.y = transform["translation"][1].get<float>();
			objectData.translation.z = transform["translation"][2].get<float>();
			objectData.rotation.x = transform["rotation"][0].get<float>() * kDegreeToRadian;
			objectData.rotation.y = transform["rotation"][1].get<float>() * kDegreeToRadian;
			objectData.rotation.z = transform["rotation"][2].get<float>() * kDegreeToRadian;
			objectData.scaling.x = transform["scaling"][0].get<float>();
			objectData.scaling.y = transform["scaling"][1].get<float>();
			objectData.scaling.z = transform["scaling"][2].get<float>();
		} else {
			// Blender出力はYとZを入れ替え、回転方向もゲーム側へ変換する。
			objectData.translation.x = transform["translation"][0].get<float>();
			objectData.translation.y = transform["translation"][2].get<float>();
			objectData.translation.z = transform["translation"][1].get<float>();
			objectData.rotation.x = -transform["rotation"][0].get<float>() * kDegreeToRadian;
			objectData.rotation.y = -transform["rotation"][2].get<float>() * kDegreeToRadian;
			objectData.rotation.z = -transform["rotation"][1].get<float>() * kDegreeToRadian;
			objectData.scaling.x = transform["scaling"][0].get<float>();
			objectData.scaling.y = transform["scaling"][2].get<float>();
			objectData.scaling.z = transform["scaling"][1].get<float>();
		}
	}

	//コライダーのパラメータ読み込み
	if (object.contains("collider"))
	{
		const nlohmann::json& collider = object["collider"];

		objectData.hasCollider = true;
		objectData.collider.type = collider["type"].get<std::string>();

		if (usesEngineCoordinates) {
			objectData.collider.center.x = collider["center"][0].get<float>();
			objectData.collider.center.y = collider["center"][1].get<float>();
			objectData.collider.center.z = collider["center"][2].get<float>();
			objectData.collider.size.x = collider["size"][0].get<float>();
			objectData.collider.size.y = collider["size"][1].get<float>();
			objectData.collider.size.z = collider["size"][2].get<float>();
		} else {
			objectData.collider.center.x = collider["center"][0].get<float>();
			objectData.collider.center.y = collider["center"][2].get<float>();
			objectData.collider.center.z = collider["center"][1].get<float>();
			objectData.collider.size.x = collider["size"][0].get<float>();
			objectData.collider.size.y = collider["size"][2].get<float>();
			objectData.collider.size.z = collider["size"][1].get<float>();
		}
	}

	if (object.contains("children"))
	{
		LoadObjects(object["children"], objectData.children, usesEngineCoordinates);
	}

	return objectData;
}

//再帰処理
void LoadObjects(
	const nlohmann::json& objects,
	std::vector<LevelLoader::ObjectData>& objectList,
	bool usesEngineCoordinates)
{
	for (const nlohmann::json& object : objects)
	{
		objectList.push_back(LoadObject(object, usesEngineCoordinates));
	}
}
}
