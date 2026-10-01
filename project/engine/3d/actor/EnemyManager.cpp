#include "EnemyManager.h"

// Sceneから借りた共通3D設定への参照を保存し、以後のSpawnで毎回渡さないようにします。
void EnemyManager::Initialize(Object3dCommon* object3dCommon)
{
	// 同じManagerを次のStageで再利用しても、前StageのEnemyを残しません。
	Clear();
	object3dCommon_ = object3dCommon;
}

// Enemy一体を作成して所有一覧へ追加し、操作用の非所有ポインタを返します。
Enemy* EnemyManager::Spawn(const Enemy::SpawnData& spawnData)
{
	if (!object3dCommon_) {
		return nullptr;
	}

	auto enemy = std::make_unique<Enemy>();
	if (!enemy->Initialize(object3dCommon_, spawnData)) {
		return nullptr;
	}

	Enemy* spawnedEnemy = enemy.get();
	enemies_.push_back(std::move(enemy));
	return spawnedEnemy;
}

// 通常のEnemyを、Stage側が細かいSpawnDataを組み立てずに一体生成します。
Enemy* EnemyManager::Spawn(
	const std::string& modelName,
	const Vector3& position,
	float colliderRadius)
{
	Enemy::SpawnData spawnData{};
	spawnData.modelName = modelName;
	spawnData.position = position;
	spawnData.colliderRadius = colliderRadius;
	return Spawn(spawnData);
}

// Stageの出現一覧を順番に生成し、失敗した一体は飛ばして成功したEnemyだけを返します。
std::vector<Enemy*> EnemyManager::SpawnAll(const std::vector<Enemy::SpawnData>& spawnDataList)
{
	std::vector<Enemy*> spawnedEnemies;
	spawnedEnemies.reserve(spawnDataList.size());
	for (const Enemy::SpawnData& spawnData : spawnDataList) {
		if (Enemy* enemy = Spawn(spawnData)) {
			spawnedEnemies.push_back(enemy);
		}
	}
	return spawnedEnemies;
}

// 所有している全Enemyを、Sceneが作った共通の描画Contextで更新します。
void EnemyManager::Update(const Object3dRenderContext& renderContext)
{
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		if (enemy) {
			enemy->Update(renderContext);
		}
	}
}

// 所有している全Enemyを生成順に描画します。
void EnemyManager::Draw() const
{
	for (const std::unique_ptr<Enemy>& enemy : enemies_) {
		if (enemy) {
			enemy->Draw();
		}
	}
}

// Stage終了時などに、所有している全Enemyをまとめて破棄します。
void EnemyManager::Clear()
{
	enemies_.clear();
}

// 生成順のEnemyを返し、範囲外アクセスをnullptrで安全に知らせます。
Enemy* EnemyManager::GetEnemy(size_t index)
{
	return index < enemies_.size() ? enemies_[index].get() : nullptr;
}

// constな呼び出し元向けに、生成順のEnemyを返します。
const Enemy* EnemyManager::GetEnemy(size_t index) const
{
	return index < enemies_.size() ? enemies_[index].get() : nullptr;
}
