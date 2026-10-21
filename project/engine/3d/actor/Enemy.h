#pragma once

#include "Collider.h"
#include <memory>
#include <string>

class Object3d;
class Object3dCommon;
class Object3dRenderContext;

// EnemyManagerが所有する一体分の敵です。見た目と球Colliderを持ち、位置を同期します。
// 現在は追跡AIや攻撃を持たず、利用側から渡された描画Contextで更新する具象クラスです。
class Enemy final
{
public:
	struct SpawnData
	{
		// 表示するモデル名です。
		std::string modelName;
		// ステージ内へ出現する位置です。
		Vector3 position{};
		// Playerや攻撃との判定に使う球の半径です。
		float colliderRadius = 0.5f;
		// trueならモデルのAnimation用データも初期化します。
		bool initializeAnimation = false;
	};

	// ManagerはEnemyを直接所有するため、前方宣言したObject3dの破棄をEnemy.cppで行います。
	Enemy();
	~Enemy();

	// モデル・位置・Colliderを作り、一体のEnemyを出現できる状態にします。
	bool Initialize(Object3dCommon* object3dCommon, const SpawnData& spawnData);
	// 利用側から借りた描画Contextで、Enemyの見た目とColliderを更新します。
	void Update(const Object3dRenderContext& renderContext);
	// 更新済みのEnemyモデルを描画します。
	void Draw() const;

	void SetPosition(const Vector3& position);
	const Vector3& GetPosition() const { return position_; }
	// Laserなど形状データを直接必要とする処理へ、球の形だけを渡します。
	const MyMath::Sphere& GetSphere() const { return collider_.GetShape(); }
	// Enemyが持つ球Colliderで、PlayerやほかのEnemyと一行で判定します。
	Collision::CollisionInfo CheckCollision(const SphereCollider& other) const { return collider_.Check(other); }
	// Enemyが持つ球Colliderで、壁・Triggerなどの箱Colliderと一行で判定します。
	Collision::CollisionInfo CheckCollision(const ObbCollider& other) const { return collider_.Check(other); }
	// 共通のCollider処理を直接使う必要がある時だけ、Enemyが所有するColliderを返します。
	const SphereCollider& GetCollider() const { return collider_; }
	Object3d* GetObject() { return object_.get(); }
	const Object3d* GetObject() const { return object_.get(); }

private:
	// Enemy自身が所有する見た目の3Dモデルです。
	std::unique_ptr<Object3d> object_;
	// Enemy自身が所有する、Playerや攻撃に使う球Colliderです。
	SphereCollider collider_{};
	// ColliderとObject3dが共有するワールド座標です。
	Vector3 position_{};
};
