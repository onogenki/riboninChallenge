#pragma once

#include <memory>
#include <vector>

namespace Engine { class Camera; }
class CarryableMirror;
class FixedMirror;
namespace Engine { class Object3d; }
class Player;
class StageLighting;
class StageMapRuntime;
class StagePauseOverlay;
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
		const std::vector<std::unique_ptr<Engine::Object3d>>& sceneObjects;
		StageMapRuntime& stageMapRuntime;
		const std::vector<std::unique_ptr<FixedMirror>>& fixedMirrors;
		FixedMirror* mirrorFloor = nullptr;
		CarryableMirror* carryableMirror = nullptr;
		Player* player = nullptr;
		Engine::SkyBox& skyBox;
		StagePauseOverlay* pauseOverlay = nullptr;
		bool showPickupPrompt = false;
	};

	// 3D ObjectとSkyBoxの行列を更新した後、拾える鏡のE案内をCamera位置へ合わせます。
	static void Prepare(const Context& context);
};
