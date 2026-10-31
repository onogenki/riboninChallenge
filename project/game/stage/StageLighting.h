#pragma once

#include "LevelLoader.h"
#include "Object3d.h"
#include <array>
#include <vector>

class StageHazardLights;
class StageLightPuzzle;

// Stage全体の照明値を所有し、JSONの三灯とLaser・危険Lightを描画用SpotLightへ合成します。
// ギミックの進行はStageLightPuzzleとStageHazardLights、GPUへの反映はRenderContextが担当します。
class StageLighting
{
public:
	// JSONにLightingがない時も同じ見た目になるよう、Stage1の初期照明を設定します。
	void InitializeDefaults();
	// 読込済みLightingを適用し、指定がない時は現在の平行光・点光源を保ち、JSON用SpotLightを消します。
	void ApplyLevel(const LevelLoader::LightingData* lighting);
	// ギミック更新後の線分を、次の描画で使うSpotLight配列へ同じ優先順で合成します。
	void UpdateLaserSpotLights(const StageLightPuzzle& lightPuzzle, const StageHazardLights& hazardLights);

	// RenderContextが描画値を一括転送するため、内部のLightを変更できない参照で渡します。
	const Engine::Object3d::DirectionalLight& GetDirectionalLight() const { return directionalLight_; }
	const Engine::Object3d::PointLight& GetPointLight() const { return pointLight_; }
	const std::array<Engine::Object3d::SpotLight, Engine::Object3d::kMaximumSpotLightCount>& GetSpotLights() const { return spotLights_; }

private:
	// 三灯照明は最大3本に固定し、残りのSpotLight枠をLaser演出へ残します。
	static constexpr size_t kStageLightingSpotLightCount = 3;

	Engine::Object3d::DirectionalLight directionalLight_{};
	Engine::Object3d::PointLight pointLight_{};
	std::vector<Engine::Object3d::SpotLight> stageSpotLights_;
	std::array<Engine::Object3d::SpotLight, Engine::Object3d::kMaximumSpotLightCount> spotLights_{};
};
