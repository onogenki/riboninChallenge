#pragma once

#include "FileHotReload.h"
#include "LevelLoader.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class DebugEntityRegistry;
class DebugSceneSelection;
class Object3d;
class Object3dCommon;
class Sprite;

// DebugSceneのscene.json読込とHot Reloadだけを担当する実行部品です。
// Objectの寿命はDebugSceneが持ち、JSON由来の範囲とEditor追加分の境界だけを管理します。
class DebugLevelRuntime
{
public:
	struct Context
	{
		Object3dCommon* object3dCommon = nullptr;
		std::vector<std::unique_ptr<Object3d>>* normalObjects = nullptr;
		const std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		const std::vector<std::unique_ptr<Sprite>>* sprites = nullptr;
		DebugEntityRegistry* entityRegistry = nullptr;
		DebugSceneSelection* selection = nullptr;
	};

	// 読むLevel名・監視ファイル・Debug初期モデル数を設定します。
	void Initialize(
		const std::string& levelFileName,
		const std::string& watchFilePath,
		size_t debugInitialObjectCount);
	// scene.jsonを読み、Debug初期モデルとEditor追加モデルを残したままJSON由来部分だけを入れ替えます。
	bool Reload(const Context& context);
	// scene.jsonが保存された時だけReloadします。
	void UpdateHotReload(const Context& context);
	// DebugSceneが初期ECS登録を終えた後、Hot Reload時のECS同期を有効にします。
	void SetEcsSyncReady(bool isReady) { isEcsSyncReady_ = isReady; }
	// 初回読込直後のファイル時刻を記録し、次フレームの誤検出を防ぎます。
	void SynchronizeWatch();
	// DebugSceneの追加モデル判定に使う、初期＋JSON由来モデル数を返します。
	size_t GetProtectedNormalObjectCount() const { return debugInitialObjectCount_ + loadedObjectCount_; }
	// Scene終了時に、次回のDebug表示へ読込状態を持ち込まないよう初期化します。
	void Reset();

private:
	// JSONツリーを再帰的に調べ、MESHだけを実行中Object3dとして生成します。
	void CreateLevelObjects(
		const std::vector<LevelLoader::ObjectData>& objectDataList,
		Object3dCommon* object3dCommon,
		std::vector<std::unique_ptr<Object3d>>& outObjects) const;

	std::string levelFileName_;
	FileHotReload hotReload_{};
	// DebugSceneが最初から生成した通常モデル数です。
	size_t debugInitialObjectCount_ = 0;
	// scene.jsonから生成した通常モデル数です。
	size_t loadedObjectCount_ = 0;
	// trueならHot Reload後にECS Entity一覧も同期します。
	bool isEcsSyncReady_ = false;
};
