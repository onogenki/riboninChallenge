#pragma once

#include "Enemy.h"
#include <memory>
#include <vector>

class Object3dCommon;
class Object3dRenderContext;

// Enemyの一覧と寿命を所有し、Sceneから出現処理を依頼されるクラスです。
// Stageは個別Enemyをnewせず、必ずEnemyManager::Spawnで追加します。
class EnemyManager
{
public:
	// Scene開始時にEnemy一覧を空にしてから、モデル生成で使う共通3D設定を受け取ります。
	// Object3dCommonは共有のSingletonです。SceneもEnemyManagerも借りるだけで、解放しません。
	void Initialize(Object3dCommon* object3dCommon);
	// SpawnDataに従ってEnemyを一体生成します。失敗時はnullptrです。
	Enemy* Spawn(const Enemy::SpawnData& spawnData);
	// 通常のEnemyを、モデル名・出現位置・Collider半径だけで一体生成します。失敗時はnullptrです。
	Enemy* Spawn(
		const std::string& modelName,
		const Vector3& position,
		float colliderRadius = 0.5f);
	// Stageが持つ出現データ一覧からEnemyをまとめて生成し、生成成功分の操作用ポインタを返します。
	std::vector<Enemy*> SpawnAll(const std::vector<Enemy::SpawnData>& spawnDataList);
	// 管理中の全Enemyを、Sceneが一度作った共通の描画Contextで更新します。
	void Update(const Object3dRenderContext& renderContext);
	// 管理中の全Enemyを描画します。
	void Draw() const;
	// Scene終了時やStage再読込時に、管理中のEnemyを全て解放します。
	void Clear();

	// 現在Managerが所有しているEnemy数を返します。
	size_t GetCount() const { return enemies_.size(); }
	// 生成順のEnemyを返します。範囲外ならnullptrです。
	Enemy* GetEnemy(size_t index);
	// constな呼び出し元向けに、生成順のEnemyを返します。範囲外ならnullptrです。
	const Enemy* GetEnemy(size_t index) const;

private:
	// EnemyモデルをFactory経由で作るための共通3D設定です。共有Singletonへの非所有参照です。
	Object3dCommon* object3dCommon_ = nullptr;
	// EnemyManagerだけがEnemyの寿命を所有します。
	std::vector<std::unique_ptr<Enemy>> enemies_;
};
