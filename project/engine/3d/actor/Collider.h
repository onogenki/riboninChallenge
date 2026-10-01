#pragma once

#include "Collision.h"

// 3Dゲーム物体が持つ当たり判定の共通土台です。
// PlayerやEnemyはColliderそのものではなく、Colliderを所有するゲーム物体です。
class Collider
{
public:
	enum class ShapeType
	{
		kSphere,
		kObb,
	};

	virtual ~Collider() = default;

	ShapeType GetShapeType() const { return shapeType_; }
	bool IsEnabled() const { return isEnabled_; }
	void SetEnabled(bool isEnabled) { isEnabled_ = isEnabled; }
	bool IsTrigger() const { return isTrigger_; }
	void SetTrigger(bool isTrigger) { isTrigger_ = isTrigger; }

protected:
	explicit Collider(ShapeType shapeType) : shapeType_(shapeType) {}

private:
	ShapeType shapeType_;
	// falseなら判定対象にせず、見た目だけ残せます。
	bool isEnabled_ = true;
	// Trigger扱いの印です。Checkは重なりを返し、SphereCollider自身のResolveは押し戻しません。
	// GetShapeで取り出した生のOBBにはこの印がないため、床・壁一覧へ入れるかは収集側が判断します。
	bool isTrigger_ = false;
};

class ObbCollider;

// 球形の当たり判定です。Playerや空中を動くEnemyに使います。
class SphereCollider final : public Collider
{
public:
	SphereCollider();
	SphereCollider(const Vector3& center, float radius);

	// 中心と半径を同時に設定し、生成直後の球Colliderを一行で準備します。
	void SetShape(const Vector3& center, float radius) { sphere_ = { center, radius }; }
	void SetCenter(const Vector3& center) { sphere_.center = center; }
	void SetRadius(float radius) { sphere_.radius = radius; }
	// 指定した球形状と重なっているかを調べ、法線とめり込み量も返します。
	Collision::CollisionInfo Check(const MyMath::Sphere& other) const;
	// 二つの球Colliderの有効状態を確認してから、球同士の判定を行います。
	Collision::CollisionInfo Check(const SphereCollider& other) const;
	// 球Colliderと箱Colliderの有効状態を確認してから、球対箱の判定を行います。
	Collision::CollisionInfo Check(const ObbCollider& other) const;
	// 指定OBBと重なっているかを調べ、法線とめり込み量も返します。
	Collision::CollisionInfo Check(const MyMath::OBB& other) const;
	// 複数の床・壁OBBから球を押し戻し、球の中心・速度・接地状態をまとめて更新します。
	Collision::SphereObbResolution ResolveSolidObbs(
		Vector3& velocity,
		const std::vector<MyMath::OBB>& solidObbs,
		int solveCount = 3,
		float groundNormalThreshold = 0.5f);
	const MyMath::Sphere& GetShape() const { return sphere_; }

private:
	MyMath::Sphere sphere_{};
};

// 回転と拡大率を反映できる直方体の当たり判定です。床・壁・置物に使います。
class ObbCollider final : public Collider
{
public:
	ObbCollider();

	// モデル原点から見たColliderの中心と半分の大きさを設定します。
	void SetLocalShape(const Vector3& localCenter, const Vector3& localHalfSize);
	// Object3dのTransformを受け取り、実際に判定するワールドOBBを更新します。
	void SyncTransform(const Transform& transform);
	// 指定OBBと重なっているかを調べ、法線とめり込み量も返します。
	Collision::CollisionInfo Check(const MyMath::OBB& other) const;
	// 箱側から呼んでも、球Colliderとの重なりを一行で判定できます。
	// 返す法線は、この箱Colliderを球Colliderから離す向きです。
	Collision::CollisionInfo Check(const SphereCollider& other) const;
	// 二つの箱Colliderの有効状態を確認してから、箱同士の判定を行います。
	Collision::CollisionInfo Check(const ObbCollider& other) const;
	const MyMath::OBB& GetShape() const { return obb_; }

private:
	Vector3 localCenter_{};
	Vector3 localHalfSize_{ 0.5f, 0.5f, 0.5f };
	MyMath::OBB obb_{};
};
