#include "Stage1.h"

using namespace MyMath;

// Laserと危険Lightの線分を、壁や床を照らすSpotLightへ変換します。
void Stage1::UpdateLaserSpotLights()
{
	// 見えるLaser線分と別にSpotLightを作り、光線が近くの壁・床を実際に照らすようにします。
	// 前フレームのLightが残らないよう、まず全要素を無効化します。
	spotLights_.fill({});
	size_t lightIndex = 0;
	// 最初にJSONで決めたキー・フィル・バックライトを入れ、残りの枠をLaser用に使います。
	for (const Object3d::SpotLight& stageSpotLight : stageSpotLights_) {
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
			Object3d::SpotLight& spotLight = spotLights_[lightIndex++];
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
	addSegmentsAsSpotLights(lightPuzzle_.GetChargeLaser().GetSegments(), { 0.05f, 0.95f, 1.00f, 1.0f }, 3.5f);
	addSegmentsAsSpotLights(lightPuzzle_.GetDoorLaser().GetSegments(), { 1.00f, 0.38f, 0.05f, 1.0f }, 3.5f);
	addSegmentsAsSpotLights(hazardLights_.GetCeilingSweepSegments(), { 0.95f, 0.20f, 1.00f, 1.0f }, 3.2f);
	addSegmentsAsSpotLights(hazardLights_.GetHorizontalMoveSegments(), { 1.00f, 0.82f, 0.10f, 1.0f }, 3.0f);
	addSegmentsAsSpotLights(hazardLights_.GetBottomPulseSegments(), { 0.15f, 0.55f, 1.00f, 1.0f }, 3.4f);
	addSegmentsAsSpotLights(hazardLights_.GetOrbitSegments(), { 0.20f, 1.00f, 0.35f, 1.0f }, 2.8f);
}
