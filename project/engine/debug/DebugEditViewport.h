#pragma once

#include "SceneEditor.h"
#include <functional>
#include <memory>
#include <vector>

class Camera;
class Object3d;
class Sprite;

// DebugSceneの3D・2D Edit Viewを表示するDebug用UI部品です。
// 実際のObject・Spriteと選択状態の所有者はDebugSceneで、この部品は選択UIだけを担当します。
class DebugEditViewport
{
public:
	struct Context
	{
		Camera* camera = nullptr;
		std::vector<std::unique_ptr<Object3d>>* normalObjects = nullptr;
		std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		std::vector<std::unique_ptr<Sprite>>* sprites = nullptr;
		// Preview中はEdit Viewで通常SceneのObjectを選択させません。
		bool isAssetPreviewActive = false;
		// DebugSceneが所有するViewのCamera・選択表示状態です。
		SceneEditor::ViewportState* modelViewportState = nullptr;
		SceneEditor::SpriteViewportState* spriteViewportState = nullptr;
		// 現在のScene選択を、Viewportの選択表示へ反映する値です。
		bool hasSelectedObject = false;
		bool selectedObjectIsAnimation = false;
		size_t selectedObjectIndex = 0;
		bool hasSelectedSprite = false;
		size_t selectedSpriteIndex = 0;
		// Viewportで選択が変わった時に、Scene側の選択状態を更新する窓口です。
		std::function<void(bool, size_t)> selectObject;
		std::function<void()> clearObjectSelection;
		std::function<void(size_t)> selectSprite;
		std::function<void()> clearSpriteSelection;
	};

	// 3DモデルとSpriteのEdit Viewを描画し、選択変更をSceneへ返します。
	static void Draw(const Context& context);

private:
	// 通常・Animationモデルを一つの3D Viewへ並べます。
	static void DrawModelViewport(const Context& context);
	// Spriteを2D Viewへ並べます。
	static void DrawSpriteViewport(const Context& context);
};
