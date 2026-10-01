#include "EcsSystems.h"
#include "EcsWorld.h"
#include "Object3d.h"
#include <cmath>
#include <vector>

namespace {

// スケール値の符号を除いてAABBの大きさを求める。
float Abs(float value)
{
	return value < 0.0f ? -value : value;
}

// 2つの軸平行境界ボックスが全軸で重なっているか判定する。
bool IsOverlapping(const MyMath::AABB& first, const MyMath::AABB& second)
{
	return first.min.x <= second.max.x && first.max.x >= second.min.x &&
		first.min.y <= second.max.y && first.max.y >= second.min.y &&
		first.min.z <= second.max.z && first.max.z >= second.min.z;
}

} // namespace

namespace Ecs {

void TransformSyncSystem::Update(World& world)
{
	// 描画オブジェクトのTransformをECS側のコンポーネントへ同期する。
	for (const auto& [entity, model] : world.models_) {
		if (!model.object) {
			continue;
		}
		world.transforms_[entity].value = model.object->GetTransform();
	}
}

void BoxCollisionSystem::Update(World& world)
{
	// 有効な各コライダーのローカル形状をワールド座標のAABBへ変換する。
	std::vector<Entity> activeColliders;
	for (auto& [entity, collider] : world.boxColliders_) {
		collider.isColliding = false;
		if (!collider.enabled) {
			continue;
		}
		const auto transformIt = world.transforms_.find(entity);
		if (transformIt == world.transforms_.end()) {
			continue;
		}
		const Transform& transform = transformIt->second.value;
		const Vector3 scaledCenter{
			collider.center.x * transform.scale.x,
			collider.center.y * transform.scale.y,
			collider.center.z * transform.scale.z
		};
		const Vector3 scaledHalfExtents{
			Abs(collider.halfExtents.x * transform.scale.x),
			Abs(collider.halfExtents.y * transform.scale.y),
			Abs(collider.halfExtents.z * transform.scale.z)
		};
		const Vector3 center{
			transform.translate.x + scaledCenter.x,
			transform.translate.y + scaledCenter.y,
			transform.translate.z + scaledCenter.z
		};
		collider.worldBounds.min = {
			center.x - scaledHalfExtents.x,
			center.y - scaledHalfExtents.y,
			center.z - scaledHalfExtents.z
		};
		collider.worldBounds.max = {
			center.x + scaledHalfExtents.x,
			center.y + scaledHalfExtents.y,
			center.z + scaledHalfExtents.z
		};
		activeColliders.push_back(entity);
	}

	// 総当たりでペアを判定し、重なった両方の状態を更新する。
	for (size_t first = 0; first < activeColliders.size(); ++first) {
		for (size_t second = first + 1; second < activeColliders.size(); ++second) {
			BoxColliderComponent& firstCollider = world.boxColliders_.at(activeColliders[first]);
			BoxColliderComponent& secondCollider = world.boxColliders_.at(activeColliders[second]);
			if (IsOverlapping(firstCollider.worldBounds, secondCollider.worldBounds)) {
				firstCollider.isColliding = true;
				secondCollider.isColliding = true;
			}
		}
	}
}

} // namespace Ecs
