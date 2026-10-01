#pragma once

#include "../ecs/EcsWorld.h"
#include <memory>
#include <string>
#include <vector>

class Object3d;
class Sprite;

// DebugScene内のObject・SpriteをECS Entityとして登録し、選択中Entityを管理する部品です。
// ObjectとSpriteそのものは所有せず、Sceneが生存中の一覧を毎フレーム渡して同期します。
class DebugEntityRegistry
{
public:
	// 新しく追加したモデル・SpriteをEntityとして登録し、Inspector選択をそのEntityへ移します。
	void RegisterModel(Object3d* object, const std::string& sourceFile, bool isAnimated);
	void RegisterSprite(Sprite* sprite, const std::string& sourceFile);
	// 初期配置やLevel再読込の要素を、Inspector選択を変えずに登録します。
	void RegisterInitialModel(Object3d* object, const std::string& sourceFile, bool isAnimated);
	void RegisterInitialSprite(Sprite* sprite, const std::string& sourceFile);
	// 現在のScene一覧にない古いEntityを外し、ECS Systemを更新します。
	void Synchronize(
		const std::vector<std::unique_ptr<Object3d>>& normalObjects,
		const std::vector<std::unique_ptr<Object3d>>& animationObjects,
		const std::vector<std::unique_ptr<Sprite>>& sprites);
	// 選択した見た目Objectに対応するEntityをInspector選択にします。
	void SelectObject(const Object3d* object);
	void SelectSprite(const Sprite* sprite);
	// Worldと選択中Entityへの参照を、InspectorやCollider表示へ渡します。
	Ecs::World& GetWorld() { return world_; }
	const Ecs::World& GetWorld() const { return world_; }
	Ecs::Entity& GetSelectedEntity() { return selectedEntity_; }
	const Ecs::Entity& GetSelectedEntity() const { return selectedEntity_; }
	// Scene終了時に登録済みEntityと選択状態をまとめて消去します。
	void Clear();

private:
	Ecs::World world_{};
	Ecs::Entity selectedEntity_ = Ecs::kInvalidEntity;
};
