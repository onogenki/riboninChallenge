#include "StageLighting.h"

#include "StageHazardLights.h"
#include "StageLightPuzzle.h"
#include "MyMath.h"

using namespace MyMath;

// JSONにLightingがない場合も使える、Stage1の従来の初期照明を設定します。
void StageLighting::InitializeDefaults()
{
	directionalLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLight_.direction = Normalize({ 0.5f, -1.0f, 0.5f });
	directionalLight_.intensity = 0.3f;
	directionalLight_.ambientColor = { 1.0f, 1.0f, 1.0f };
	directionalLight_.ambientIntensity = 0.0f;
	pointLight_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLight_.position = { 0.0f, 3.0f, -2.0f };
	pointLight_.intensity = 5.0f;
	pointLight_.radius = 20.0f;
	pointLight_.decay = 1.0f;
}

// JSONのLighting値だけをStage全体のLightへ反映し、指定がない時は現在の平行光・点光源を保ちます。
void StageLighting::ApplyLevel(const LevelLoader::LightingData* lighting)
{
	stageSpotLights_.clear();
	if (!lighting) {
		return;
	}

	directionalLight_.color = {
		lighting->directionalColor.x,
		lighting->directionalColor.y,
		lighting->directionalColor.z,
		1.0f,
	};
	directionalLight_.direction = Normalize(lighting->directionalDirection);
	directionalLight_.intensity = lighting->directionalIntensity;
	directionalLight_.ambientColor = lighting->ambientColor;
	directionalLight_.ambientIntensity = lighting->ambientIntensity;
	pointLight_.color = {
		lighting->pointColor.x,
		lighting->pointColor.y,
		lighting->pointColor.z,
		1.0f,
	};
	pointLight_.position = lighting->pointPosition;
	pointLight_.intensity = lighting->pointIntensity;
	pointLight_.radius = lighting->pointRadius;
	pointLight_.decay = lighting->pointDecay;
	for (const LevelLoader::SpotLightData& spotLightData : lighting->spotLights) {
		if (stageSpotLights_.size() >= kStageLightingSpotLightCount) {
			break;
		}
		if (spotLightData.intensity <= 0.0f || spotLightData.distance <= 0.0f) {
			continue;
		}

		Engine::Object3d::SpotLight spotLight{};
		spotLight.color = {
			spotLightData.color.x,
			spotLightData.color.y,
			spotLightData.color.z,
			1.0f,
		};
		spotLight.position = spotLightData.position;
		spotLight.intensity = spotLightData.intensity;
		spotLight.direction = Normalize(spotLightData.direction);
		spotLight.distance = spotLightData.distance;
		spotLight.decay = spotLightData.decay;
		spotLight.cosAngle = spotLightData.cosAngle;
		spotLight.cosFalloffStart = spotLightData.cosFalloffStart;
		stageSpotLights_.push_back(spotLight);
	}
}

// Laserと危険Lightの線分を、壁や床を照らす従来と同じ優先順のSpotLightへ変換します。
void StageLighting::UpdateLaserSpotLights(
	const StageLightPuzzle& lightPuzzle,
	const StageHazardLights& hazardLights)
{
	// 見えるLaser線分と別にSpotLightを作り、光線が近くの壁・床を実際に照らすようにします。
	// 前フレームのLightが残らないよう、まず全要素を無効化します。
	spotLights_.fill({});
	size_t lightIndex = 0;
	// 最初にJSONで決めたキー・フィル・バックライトを入れ、残りの枠をLaser用に使います。
	for (const Engine::Object3d::SpotLight& stageSpotLight : stageSpotLights_) {
		if (lightIndex >= spotLights_.size()) {
			break;
		}
		spotLights_[lightIndex++] = stageSpotLight;
	}
	const auto addSegmentsAsSpotLights =
		[this, &lightIndex](
			const std::vector<LaserSegment>& segments,
			const Vector4& color,
			float intensity)
	{
		for (const LaserSegment& segment : segments) {
			if (lightIndex >= spotLights_.size()) {
				return;
			}
			const Vector3 segmentDirection{
				segment.end.x - segment.start.x,
				segment.end.y - segment.start.y,
				segment.end.z - segment.start.z,
			};
			const float segmentLength = Length(segmentDirection);
			if (segmentLength <= 0.05f) {
				continue;
			}

			// Laserの始点・方向・遮蔽物までの長さを、そのまま円錐Lightへ使います。
			Engine::Object3d::SpotLight& spotLight = spotLights_[lightIndex++];
			spotLight.color = color;
			spotLight.position = segment.start;
			spotLight.direction = Normalize(segmentDirection);
			spotLight.intensity = intensity;
			spotLight.distance = segmentLength + 0.35f;
			spotLight.decay = 1.20f;
			spotLight.cosAngle = 0.82f;
			spotLight.cosFalloffStart = 0.96f;
		}
	};

	// 通常のPuzzle Light、Door Light、四種類の危険Lightを同じ仕組みで照明へ反映します。
	addSegmentsAsSpotLights(lightPuzzle.GetChargeLaser().GetSegments(), { 0.05f, 0.95f, 1.00f, 1.0f }, 3.5f);
	addSegmentsAsSpotLights(lightPuzzle.GetDoorLaser().GetSegments(), { 1.00f, 0.38f, 0.05f, 1.0f }, 3.5f);
	addSegmentsAsSpotLights(hazardLights.GetCeilingSweepSegments(), { 0.95f, 0.20f, 1.00f, 1.0f }, 3.2f);
	addSegmentsAsSpotLights(hazardLights.GetHorizontalMoveSegments(), { 1.00f, 0.82f, 0.10f, 1.0f }, 3.0f);
	addSegmentsAsSpotLights(hazardLights.GetBottomPulseSegments(), { 0.15f, 0.55f, 1.00f, 1.0f }, 3.4f);
	addSegmentsAsSpotLights(hazardLights.GetOrbitSegments(), { 0.20f, 1.00f, 0.35f, 1.0f }, 2.8f);
}
