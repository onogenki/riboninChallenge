#pragma once

#include "StageMapRuntime.h"
#include <memory>
#include <vector>

class Camera;
class CarryableMirror;
class DirectXCommon;
class FixedMirror;
class Object3d;
class Object3dCommon;
class Player;
class StageHazardLights;
class StageLightPuzzle;

// 固定鏡ごとの反射Cameraで、SceneをRenderTextureへ描画するStage専用Rendererです。
// Scene内Objectの所有やCamera計算は持たず、Stage1が渡す描画対象だけを使用します。
class StageReflectionRenderer
{
public:
	struct Context
	{
		DirectXCommon* directXCommon = nullptr;
		Object3dCommon* object3dCommon = nullptr;
		Camera* activeCamera = nullptr;
		const std::vector<std::unique_ptr<Object3d>>* sceneObjects = nullptr;
		StageMapRuntime* stageMapRuntime = nullptr;
		const std::vector<std::unique_ptr<FixedMirror>>* fixedMirrors = nullptr;
		FixedMirror* mirrorFloor = nullptr;
		CarryableMirror* carryableMirror = nullptr;
		Player* player = nullptr;
		const StageLightPuzzle* lightPuzzle = nullptr;
		const StageHazardLights* hazardLights = nullptr;
	};

	// 一フレームに一枚だけ鏡を選び、反射Textureを更新します。
	static void DrawOne(Context& context, size_t& reflectionUpdateCursor);

private:
	// 反射Cameraで書き換えた行列を、通常Game Cameraの行列へ戻します。
	static void RestoreMainCameraMatrices(const Context& context);
};
