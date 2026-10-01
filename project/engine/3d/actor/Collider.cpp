#include "Collider.h"

// 半径0の球Colliderを作り、後から位置と半径を設定できる状態にします。
SphereCollider::SphereCollider()
	: Collider(ShapeType::kSphere)
{
}

// 指定した中心・半径の球Colliderを作ります。
SphereCollider::SphereCollider(const Vector3& center, float radius)
	: Collider(ShapeType::kSphere)
	, sphere_{ center, radius }
{
}

// 有効な球Colliderだけ、指定球との重なり情報を共通Collision処理から取得します。
Collision::CollisionInfo SphereCollider::Check(const MyMath::Sphere& other) const
{
	if (!IsEnabled()) {
		return {};
	}
	return Collision::SphereSphere(sphere_, other);
}

// どちらかが無効なら判定せず、二つの球Colliderを一行で判定できるようにします。
Collision::CollisionInfo SphereCollider::Check(const SphereCollider& other) const
{
	if (!IsEnabled() || !other.IsEnabled()) {
		return {};
	}
	return Collision::SphereSphere(sphere_, other.sphere_);
}

// 球・箱のColliderがどちらも有効な時だけ、形状を取り出さずに一行で判定します。
Collision::CollisionInfo SphereCollider::Check(const ObbCollider& other) const
{
	if (!IsEnabled() || !other.IsEnabled()) {
		return {};
	}
	return Collision::SphereOBB(sphere_, other.GetShape());
}

// 有効な球Colliderだけ、指定OBBとの重なり情報を共通Collision処理から取得します。
Collision::CollisionInfo SphereCollider::Check(const MyMath::OBB& other) const
{
	if (!IsEnabled()) {
		return {};
	}
	return Collision::SphereOBB(sphere_, other);
}

// 球の現在中心を使い、共通Collision処理へ床・壁との押し戻しを依頼します。
Collision::SphereObbResolution SphereCollider::ResolveSolidObbs(
	Vector3& velocity,
	const std::vector<MyMath::OBB>& solidObbs,
	int solveCount,
	float groundNormalThreshold)
{
	if (!IsEnabled() || IsTrigger()) {
		return {};
	}
	return Collision::ResolveSphereObbs(
		sphere_,
		sphere_.center,
		velocity,
		solidObbs,
		solveCount,
		groundNormalThreshold);
}

// 単位サイズの箱Colliderを作り、後からモデルの情報を設定できる状態にします。
ObbCollider::ObbCollider()
	: Collider(ShapeType::kObb)
{
}

// モデルのローカル座標で、箱Colliderの中心と半分の大きさを記録します。
void ObbCollider::SetLocalShape(
	const Vector3& localCenter,
	const Vector3& localHalfSize)
{
	localCenter_ = localCenter;
	localHalfSize_ = localHalfSize;
}

// モデルの移動・回転・拡大率を反映し、判定に使うワールドOBBを更新します。
void ObbCollider::SyncTransform(const Transform& transform)
{
	obb_ = Collision::MakeOBB(transform, localCenter_, localHalfSize_);
}

// 有効なOBB Colliderだけ、指定OBBとの重なり情報を共通Collision処理から取得します。
Collision::CollisionInfo ObbCollider::Check(const MyMath::OBB& other) const
{
	if (!IsEnabled()) {
		return {};
	}
	return Collision::ObbObb(obb_, other);
}

// 箱側から球へ判定した時も、球側から呼ぶ時と同じCollider APIを使えるようにします。
Collision::CollisionInfo ObbCollider::Check(const SphereCollider& other) const
{
	if (!IsEnabled() || !other.IsEnabled()) {
		return {};
	}

	// SphereOBBは球を押し出す向きなので、箱を押し出す向きへ反転します。
	Collision::CollisionInfo result = Collision::SphereOBB(other.GetShape(), obb_);
	if (result.isCollision) {
		result.normal = MyMath::Multiply(-1.0f, result.normal);
	}
	return result;
}

// 二つの箱Colliderが有効な時だけ、形状を取り出さずに一行で判定します。
Collision::CollisionInfo ObbCollider::Check(const ObbCollider& other) const
{
	if (!IsEnabled() || !other.IsEnabled()) {
		return {};
	}
	return Collision::ObbObb(obb_, other.obb_);
}
