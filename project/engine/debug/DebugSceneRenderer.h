#pragma once

#include <memory>
#include <vector>

namespace Engine { class Camera; }
class DirectXCommon;
class Object3d;
class Object3dCommon;
namespace Engine { class SkyBox; }
namespace Engine { class Sprite; }
namespace Engine { class SpriteCommon; }

// DebugSceneの通常描画とPreview描画を切り替える、Debug専用の描画部品です。
// モデルやSpriteを所有せず、Sceneが所有する描画対象だけを受け取ります。
class DebugSceneRenderer
{
public:
	struct Context
	{
		DirectXCommon* directXCommon = nullptr;
		Object3dCommon* object3dCommon = nullptr;
		Engine::SpriteCommon* spriteCommon = nullptr;
		Engine::Camera* camera = nullptr;
		Object3d* previewObject = nullptr;
		Engine::Sprite* previewSprite = nullptr;
		std::vector<std::unique_ptr<Object3d>>* normalObjects = nullptr;
		std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		Object3d* handWeapon = nullptr;
		Engine::SkyBox* skyBox = nullptr;
		std::vector<std::unique_ptr<Engine::Sprite>>* sprites = nullptr;
	};

	// 3D背景・Particle・Sprite・PostEffect・ImGuiの順でDebug Sceneを描画します。
	static void Draw(const Context& context);
};
