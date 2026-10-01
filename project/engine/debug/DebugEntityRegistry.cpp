#include "DebugEntityRegistry.h"

#include "Object3d.h"
#include "Sprite.h"

// 新しく追加したモデルをEntityとして登録し、Inspector選択をそのEntityへ移します。
void DebugEntityRegistry::RegisterModel(Object3d* object, const std::string& sourceFile, bool isAnimated)
{
	selectedEntity_ = world_.CreateModelEntity(object, sourceFile, isAnimated);
}

// 新しく追加したSpriteをEntityとして登録し、Inspector選択をそのEntityへ移します。
void DebugEntityRegistry::RegisterSprite(Sprite* sprite, const std::string& sourceFile)
{
	selectedEntity_ = world_.CreateSpriteEntity(sprite, sourceFile);
}

// 初期配置やLevel再読込のモデルを、Inspector選択を変えずに登録します。
void DebugEntityRegistry::RegisterInitialModel(Object3d* object, const std::string& sourceFile, bool isAnimated)
{
	world_.CreateModelEntity(object, sourceFile, isAnimated);
}

// 初期配置やLevel再読込のSpriteを、Inspector選択を変えずに登録します。
void DebugEntityRegistry::RegisterInitialSprite(Sprite* sprite, const std::string& sourceFile)
{
	world_.CreateSpriteEntity(sprite, sourceFile);
}

// 現在のScene一覧にない古いEntityを外し、ECS Systemを更新します。
void DebugEntityRegistry::Synchronize(
	const std::vector<std::unique_ptr<Object3d>>& normalObjects,
	const std::vector<std::unique_ptr<Object3d>>& animationObjects,
	const std::vector<std::unique_ptr<Sprite>>& sprites)
{
	std::vector<Object3d*> currentObjects;
	currentObjects.reserve(normalObjects.size() + animationObjects.size());
	for (const auto& object : normalObjects) {
		if (object) {
			currentObjects.push_back(object.get());
		}
	}
	for (const auto& object : animationObjects) {
		if (object) {
			currentObjects.push_back(object.get());
		}
	}

	std::vector<Sprite*> currentSprites;
	currentSprites.reserve(sprites.size());
	for (const auto& sprite : sprites) {
		if (sprite) {
			currentSprites.push_back(sprite.get());
		}
	}

	world_.PruneMissingBindings(currentObjects, currentSprites);
	world_.UpdateSystems();
	if (!world_.IsAlive(selectedEntity_)) {
		selectedEntity_ = Ecs::kInvalidEntity;
	}
}

// 選択した見た目Objectに対応するEntityをInspector選択にします。
void DebugEntityRegistry::SelectObject(const Object3d* object)
{
	selectedEntity_ = world_.FindEntity(object);
}

// 選択したSpriteに対応するEntityをInspector選択にします。
void DebugEntityRegistry::SelectSprite(const Sprite* sprite)
{
	selectedEntity_ = world_.FindEntity(sprite);
}

// Scene終了時に登録済みEntityと選択状態をまとめて消去します。
void DebugEntityRegistry::Clear()
{
	world_.Clear();
	selectedEntity_ = Ecs::kInvalidEntity;
}
