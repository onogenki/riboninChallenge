#pragma once

#include <memory>
#include <vector>

class Camera;
class DirectXCommon;
class Object3d;
class Object3dCommon;
class SkyBox;
class Sprite;
class SpriteCommon;

// DebugSceneの通常描画とPreview描画を切り替える、Debug専用の描画部品です。
// モデルやSpriteを所有せず、Sceneが所有する描画対象だけを受け取ります。
class DebugSceneRenderer
{
public:
	struct Context
	{
		DirectXCommon* directXCommon = nullptr;
		Object3dCommon* object3dCommon = nullptr;
		SpriteCommon* spriteCommon = nullptr;
		Camera* camera = nullptr;
		Object3d* previewObject = nullptr;
		Sprite* previewSprite = nullptr;
		std::vector<std::unique_ptr<Object3d>>* normalObjects = nullptr;
		std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		Object3d* handWeapon = nullptr;
		SkyBox* skyBox = nullptr;
		std::vector<std::unique_ptr<Sprite>>* sprites = nullptr;
	};

	// 3D背景・Particle・Sprite・PostEffect・ImGuiの順でDebug Sceneを描画します。
	static void Draw(const Context& context);
};
