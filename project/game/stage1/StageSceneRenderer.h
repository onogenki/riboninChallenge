#pragma once

#include "StageMapRuntime.h"
#include <memory>
#include <vector>

class Camera;
class CarryableMirror;
class FixedMirror;
class Object3d;
class Object3dCommon;
class Player;
class SkyBox;
class StageHazardLights;
class StageLightPuzzle;

// Stage1の通常Game Viewへ、所有済みObjectを決まった描画順で表示するRendererです。
// Sceneの進行・Objectの寿命・Camera計算は持たず、Stage1が渡す描画対象を使うだけです。
class StageSceneRenderer
{
public:
	struct Context
	{
		Object3dCommon* object3dCommon = nullptr;
		Camera* activeCamera = nullptr;
		SkyBox* skyBox = nullptr;
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
