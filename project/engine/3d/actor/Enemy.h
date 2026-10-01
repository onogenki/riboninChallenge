#pragma once

#include "Collider.h"
#include <memory>
#include <string>

class Object3d;
class Object3dCommon;
class Object3dRenderContext;

// EnemyManagerが所有する、一体分の敵の共通データです。
// 敵ごとの追跡AIや攻撃は、将来このクラスを継承した個別Enemyへ追加します。
class Enemy
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

	// 前方宣言したObject3dを安全に扱うため、生成・破棄の実装はEnemy.cppに置きます。
	Enemy();
	virtual ~Enemy();

	// モデル・位置・Colliderを作り、一体のEnemyを出現できる状態にします。
	bool Initialize(Object3dCommon* object3dCommon, const SpawnData& spawnData);
	// Sceneが一度作った描画Contextを使い、Enemyの見た目とColliderを更新します。
	virtual void Update(const Object3dRenderContext& renderContext);
	// 更新済みのEnemyモデルを描画します。
	virtual void Draw() const;

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
