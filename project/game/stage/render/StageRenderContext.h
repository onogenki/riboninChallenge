#pragma once

#include <memory>
#include <vector>

namespace Engine { class Camera; }
class CarryableMirror;
class FixedMirror;
class Object3d;
class Player;
class StageLighting;
class StageMapRuntime;
namespace Engine { class SkyBox; }

// Stageが所有する描画対象を借り、Camera・Lightと行列の準備を同じ順序で行います。
// Objectの所有やDrawは行わず、Sceneから渡された一Frame分の値だけを使います。
class StageRenderContext
{
public:
	struct Context
	{
		Engine::Camera* activeCamera = nullptr;
		const StageLighting& lighting;
		const std::vector<std::unique_ptr<Object3d>>& sceneObjects;
		StageMapRuntime& stageMapRuntime;
		const std::vector<std::unique_ptr<FixedMirror>>& fixedMirrors;
		FixedMirror* mirrorFloor = nullptr;
		CarryableMirror* carryableMirror = nullptr;
		Player* player = nullptr;
		Engine::SkyBox& skyBox;
	};

	// Stage内の全3D Objectへ同じCamera・Lightを反映し、最後にSkyBoxの行列を更新します。
	static void Prepare(const Context& context);
};
