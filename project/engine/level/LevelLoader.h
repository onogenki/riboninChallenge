#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Vector3.h"

// Blenderのレベルエディタが出力したJSONを、ゲームで扱うレベルデータへ変換する。
class LevelLoader
{

public:
	struct ColliderData
	{
		// コライダー種別とローカル座標系での形状情報。
		std::string type;
		Vector3 center{};
		Vector3 size{};
	};

	struct CameraAreaData
	{
		// Area内で使用する三人称Cameraの距離・縦角度・通常時の視野角。
		float distance = 10.77f;
		float pitch = 0.38050638f;
		float fovY = 0.45f;
	};

	// ステージ開始演出で使う、Playerの落下とCamera軌道の設定です。
	struct StageStartData
	{
		float duration = 2.5f;
		float playerAirHeight = 8.0f;
		float cameraFrontDistance = 10.0f;
		float cameraFrontHeight = 6.0f;
		float cameraOrbitAngle = 3.14159265f;
		float cameraHandoffDuration = 0.8f;
	};

	// ステージ内の一方向を照らし、キー・フィル・バックライトにも使えるSpotLightの設定です。
	struct SpotLightData
	{
		std::string name;
		Vector3 color{ 1.0f, 1.0f, 1.0f };
		Vector3 position{};
		float intensity = 0.0f;
		Vector3 direction{ 0.0f, -1.0f, 0.0f };
		float distance = 0.0f;
		float decay = 1.0f;
		float cosAngle = 0.5f;
		float cosFalloffStart = 0.8f;
	};

	// ステージ全体の明るさを決める、共有の平行光源・点光源・SpotLight群の設定です。
	struct LightingData
	{
		Vector3 directionalColor{ 1.0f, 1.0f, 1.0f };
		Vector3 directionalDirection{ 0.5f, 1.0f, 0.5f };
		float directionalIntensity = 0.3f;
		// 直接光の向きに関係なく、影側を見える明るさへ保つ環境光です。
		Vector3 ambientColor{ 1.0f, 1.0f, 1.0f };
		float ambientIntensity = 0.0f;
		Vector3 pointColor{ 1.0f, 1.0f, 1.0f };
		Vector3 pointPosition{ 0.0f, 3.0f, -2.0f };
		float pointIntensity = 5.0f;
		float pointRadius = 20.0f;
		float pointDecay = 1.0f;
		// JSONのspot_lightsへ書いた順番で、Stage1の共有SpotLightとして使用します。
		std::vector<SpotLightData> spotLights;
	};

	struct ObjectData
	{
		// オブジェクト本体、Transform、子オブジェクトをまとめた再帰データ。
		std::string type;
		std::string name;
		std::string tag;
		std::string objectType;
		std::string fileName;
		Vector3 translation{};
		Vector3 rotation{};
		Vector3 scaling{ 1.0f, 1.0f, 1.0f };
		// イベントトリガーとイベントカメラを結び付ける情報。
		std::string eventId;
		std::string eventCameraName;
		bool hasCameraFocus = false;
		Vector3 cameraFocus{};
		// Playerが入った間だけ、通常Cameraの設定を変更する領域です。
		bool hasCameraArea = false;
		CameraAreaData cameraArea;
		// オブジェクトが移動する曲線の制御点と再生設定。
		std::vector<Vector3> controlPoints;
		float pathSpeed = 1.0f;
		bool pathLoop = true;
		bool hasCollider = false;
		ColliderData collider;
		std::vector<ObjectData> children;
	};

	struct LevelData
	{
		// シーン直下に置かれたオブジェクト一覧。
		std::string coordinateSystem = "blender";
		// シーン全体に一つだけ置く、開始演出と照明の設定です。
		bool hasStageStart = false;
		StageStartData stageStart;
		bool hasLighting = false;
		LightingData lighting;
		std::vector<ObjectData> objects;
	};

	// resources/levels配下のJSONを読み込み、失敗時はnullptrを返す。
	static std::unique_ptr<LevelData> Load(const std::string& fileName);
	// LevelDataをresources/levels配下のJSONへ保存し、失敗時はfalseを返す。
	static bool Save(const std::string& fileName, const LevelData& levelData);
};
