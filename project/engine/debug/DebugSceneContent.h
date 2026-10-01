#pragma once

#include "Object3d.h"
#include "SceneEditor.h"
#include "Vector2.h"
#include "Vector3.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Camera;
class DirectXCommon;
class Object3dCommon;
class Sprite;
class SpriteCommon;

// DebugSceneへモデル・Textureを追加する共通処理です。
// ObjectとSpriteの寿命はDebugSceneが所有し、この部品は生成と追加の順番だけを担当します。
class DebugSceneContent
{
public:
	struct Context
	{
		Object3dCommon* object3dCommon = nullptr;
		SpriteCommon* spriteCommon = nullptr;
		DirectXCommon* directXCommon = nullptr;
		Camera* camera = nullptr;
		const std::vector<SceneEditor::ShelfEntry>* modelLibrary = nullptr;
		std::vector<std::unique_ptr<Object3d>>* normalObjects = nullptr;
		std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		std::vector<std::unique_ptr<Sprite>>* sprites = nullptr;
		size_t baseNormalObjectCount = 0;
		size_t baseAnimationObjectCount = 0;
		size_t baseSpriteCount = 0;
		// Sceneが所有するECSと選択状態へ、追加・削除を通知する窓口です。
		std::function<void(Object3d*, const std::string&, bool)> registerModel;
		std::function<void(Sprite*, const std::string&)> registerSprite;
		std::function<void(bool, size_t)> selectObject;
		std::function<void(size_t)> selectSprite;
		std::function<void()> updateEcsWorld;
		std::function<void()> clearObjectSelection;
		std::function<void()> clearSpriteSelection;
	};

	// 棚の情報を使ってObject3dを生成し、必要なAnimationだけを設定します。
	// Camera・Light・行列更新は、毎フレームObject3dRenderContextがまとめて行います。
	static std::unique_ptr<Object3d> CreateObject(
		const Context& context,
		const std::string& fileName,
		bool playAnimation);
	// Camera前方または指定した3D座標へモデルを追加します。
	static bool AddModel(const Context& context, const std::string& fileName);
	static bool AddModel(const Context& context, const std::string& fileName, const Vector3& spawnPosition);
	// 画面中央または指定した2D座標へTexture Spriteを追加します。
	static bool AddTexture(const Context& context, const std::string& textureFilePath);
	static bool AddTexture(const Context& context, const std::string& textureFilePath, const Vector2& position);
	// 初期配置以外のモデル・Spriteを消し、ECSと選択状態を同期します。
	static void ClearAdded(const Context& context);
};
