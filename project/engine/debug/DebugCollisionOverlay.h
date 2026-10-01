#pragma once

#include "Object3d.h"
#include "../ecs/EcsWorld.h"
#include "../math/MyMath.h"
#include <memory>
#include <vector>

class Camera;

// 実行中ObjectとECSのColliderを、Debug用のワイヤーフレームとして表示する部品です。
// 判定結果の変更は行わず、現在の当たり判定を見える形にする責任だけを持ちます。
class DebugCollisionOverlay
{
public:
	struct Context
	{
		// DebugSceneが所有する通常モデルです。所有しません。
		const std::vector<std::unique_ptr<Object3d>>* normalObjects = nullptr;
		// DebugSceneが所有するAnimationモデルです。所有しません。
		const std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		// プレビュー中だけ表示するモデルです。所有しません。
		const Object3d* previewObject = nullptr;
		// trueならScene内モデルではなくpreviewObjectだけを表示します。
		bool isPreviewMode = false;
		// ECSで管理するBOX Collider一覧を取得します。所有しません。
		const Ecs::World* world = nullptr;
		// 3D座標をGame Viewへ投影するCameraです。所有しません。
		const Camera* camera = nullptr;
	};

	// Object3dの頂点をワールド座標へ変換し、Debug表示用のAABBを作ります。
	static bool BuildWorldAabb(const Object3d& object, MyMath::AABB& outAabb);
	// Game ViewへColliderの線を描画します。重なりは赤、通常は青で表示します。
	static void Draw(const Context& context);
};
