#pragma once

namespace Ecs {

class World;

// Scene側の描画物から現在のTransformをECSへ同期します。
class TransformSyncSystem {
public:
	static void Update(World& world);
};

// 登録されたBox Collider同士の接触状態をECSへ反映します。
class BoxCollisionSystem {
public:
	static void Update(World& world);
};

} // namespace Ecs
