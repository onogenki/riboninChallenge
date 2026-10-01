#include "Enemy.h"

#include "Object3d.h"
#include "Object3dFactory.h"
#include "Object3dRenderContext.h"

// Enemyが所有する前方宣言型Object3dを、完全な型を知る場所で生成・破棄します。
Enemy::Enemy() = default;
Enemy::~Enemy() = default;

// SpawnDataから敵一体の見た目・開始位置・球Colliderを作ります。
bool Enemy::Initialize(Object3dCommon* object3dCommon, const SpawnData& spawnData)
{
	object_ = Object3dFactory::Create(
		object3dCommon,
		spawnData.modelName,
		spawnData.initializeAnimation);
	if (!object_) {
		return false;
	}

	position_ = spawnData.position;
	collider_.SetShape(position_, spawnData.colliderRadius);
	object_->SetTranslate(position_);
	return true;
}

// Sceneが作った共通の描画Contextで見た目を更新し、Colliderを現在位置へ同期します。
void Enemy::Update(const Object3dRenderContext& renderContext)
{
	if (!object_) {
		return;
	}

	// 敵固有の移動後にも、見た目とColliderが必ず同じ位置になるようにします。
	object_->SetTranslate(position_);
	collider_.SetCenter(position_);
	renderContext.UpdateObject(*object_);
}

// 作成済みの敵モデルだけを描画します。
void Enemy::Draw() const
{
	if (object_) {
		object_->Draw();
	}
}

// 敵の位置を変え、Colliderと見た目がずれないよう同時に更新します。
void Enemy::SetPosition(const Vector3& position)
{
	position_ = position;
	collider_.SetCenter(position_);
	if (object_) {
		object_->SetTranslate(position_);
	}
}
