#pragma once

#include "LevelLoader.h"
#include <vector>

// LevelDataのタグとobject_typeを一度だけ分類し、Stage1やStage2が必要な配置物を取り出すクラスです。
// ObjectData自体はLevelDataが所有するため、このクラスはコピーせず参照用ポインタだけを保持します。
class StageMapObjectIndex
{
public:
	// LevelDataのobjectsとchildrenを最後まで調べ、用途別の一覧を作り直します。
	void Build(const LevelLoader::LevelData& levelData);
	// 編集用に、childrenを含むPlayerStartを元のLevelDataから探します。
	static LevelLoader::ObjectData* FindPlayerStartForEdit(LevelLoader::LevelData& levelData);

	// Stageに一枚だけ必要な床を返します。見つからない場合はnullptrです。
	const LevelLoader::ObjectData* GetFloor() const { return floor_; }
	// Stage開始時のPlayer位置を返します。見つからない場合はnullptrです。
	const LevelLoader::ObjectData* GetPlayerStart() const { return playerStart_; }
	// Playerが持てるMirrorの初期配置を返します。見つからない場合はnullptrです。
	const LevelLoader::ObjectData* GetCarryableMirror() const { return carryableMirror_; }
	// 固定Mirrorとして生成する配置物一覧です。
	const std::vector<const LevelLoader::ObjectData*>& GetFixedMirrors() const { return fixedMirrors_; }
	// 通常の3DモデルとしてStageMapRuntimeが生成する配置物一覧です。
	const std::vector<const LevelLoader::ObjectData*>& GetRuntimeObjects() const { return runtimeObjects_; }
	// Playerが入るとCameraを切り替えるTrigger一覧です。
	const std::vector<const LevelLoader::ObjectData*>& GetEventTriggers() const { return eventTriggers_; }
	// Triggerから呼び出すEvent Camera一覧です。
	const std::vector<const LevelLoader::ObjectData*>& GetEventCameras() const { return eventCameras_; }
	// Playerが入っている間、通常Cameraの設定を変えるArea一覧です。
	const std::vector<const LevelLoader::ObjectData*>& GetCameraAreas() const { return cameraAreas_; }

private:
	// childrenを含むObjectDataツリーを再帰的に分類します。
	void Collect(const std::vector<LevelLoader::ObjectData>& objects);

	const LevelLoader::ObjectData* floor_ = nullptr;
	const LevelLoader::ObjectData* playerStart_ = nullptr;
	const LevelLoader::ObjectData* carryableMirror_ = nullptr;
	std::vector<const LevelLoader::ObjectData*> fixedMirrors_;
	std::vector<const LevelLoader::ObjectData*> runtimeObjects_;
	std::vector<const LevelLoader::ObjectData*> eventTriggers_;
	std::vector<const LevelLoader::ObjectData*> eventCameras_;
	std::vector<const LevelLoader::ObjectData*> cameraAreas_;
};
