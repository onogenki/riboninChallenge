#pragma once

#include "StageMapRuntime.h"
#include <memory>
#include <vector>

namespace Engine { class Camera; }
class CarryableMirror;
class FixedMirror;
class Object3d;
class Object3dCommon;
class Player;
namespace Engine { class SkyBox; }
class StageHazardLights;
class StageLightPuzzle;

// Stage IDに関係なく、通常Game Viewへ所有済みObjectを決まった順で描くRendererです。
// Sceneの進行・Objectの寿命・Camera計算は持たず、Stageが渡す描画対象を使うだけです。
class StageSceneRenderer
{
public:
	struct Context
	{
		Object3dCommon* object3dCommon = nullptr;
		Engine::Camera* activeCamera = nullptr;
		Engine::SkyBox* skyBox = nullptr;
		const std::vector<std::unique_ptr<Object3d>>* sceneObjects = nullptr;
		const StageMapRuntime* stageMapRuntime = nullptr;
		const std::vector<std::unique_ptr<FixedMirror>>* fixedMirrors = nullptr;
		FixedMirror* mirrorFloor = nullptr;
		CarryableMirror* carryableMirror = nullptr;
		Player* player = nullptr;
		const StageLightPuzzle* lightPuzzle = nullptr;
		const StageHazardLights* hazardLights = nullptr;
	};

	// 夜空を背景に描いてから、部屋・JSON配置物・鏡・Player・Laserを重ねます。
	static void Draw(const Context& context);
};
