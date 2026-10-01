#include "EcsWorld.h"
#include "EcsSystems.h"
#include "Object3d.h"
#include "Sprite.h"
#include <algorithm>
#include <unordered_set>

namespace Ecs {

Entity World::CreateEntity(const std::string& name)
{
	const Entity entity = nextEntity_++;
	aliveEntities_.insert(entity);
	entities_.push_back(entity);
	names_.emplace(entity, NameComponent{ name });
	return entity;
}

void World::DestroyEntity(Entity entity)
{
	if (!IsAlive(entity)) {
		return;
	}
	aliveEntities_.erase(entity);
	names_.erase(entity);
	transforms_.erase(entity);
	models_.erase(entity);
	sprites_.erase(entity);
	boxColliders_.erase(entity);
	entities_.erase(std::remove(entities_.begin(), entities_.end(), entity), entities_.end());
}

void World::Clear()
{
	entities_.clear();
	aliveEntities_.clear();
	names_.clear();
	transforms_.clear();
	models_.clear();
	sprites_.clear();
	boxColliders_.clear();
	nextEntity_ = 1;
}

bool World::IsAlive(Entity entity) const
{
	return aliveEntities_.contains(entity);
}

Entity World::CreateModelEntity(Object3d* object, const std::string& sourceFile, bool isAnimated)
{
	if (!object) {
		return kInvalidEntity;
	}
	const Entity existing = FindEntity(object);
	if (existing != kInvalidEntity) {
		return existing;
	}
	const Entity entity = CreateEntity(sourceFile.empty() ? "3D Model" : sourceFile);
	transforms_.emplace(entity, TransformComponent{ object->GetTransform() });
	models_.emplace(entity, ModelComponent{ object, sourceFile, isAnimated });
	return entity;
}

Entity World::CreateSpriteEntity(Sprite* sprite, const std::string& sourceFile)
{
	if (!sprite) {
		return kInvalidEntity;
	}
	const Entity existing = FindEntity(sprite);
	if (existing != kInvalidEntity) {
		return existing;
	}
	const Entity entity = CreateEntity(sourceFile.empty() ? "2D Texture" : sourceFile);
	sprites_.emplace(entity, SpriteComponent{ sprite, sourceFile });
	return entity;
}

Entity World::FindEntity(const Object3d* object) const
{
	for (const auto& [entity, model] : models_) {
		if (model.object == object) {
			return entity;
		}
	}
	return kInvalidEntity;
}

Entity World::FindEntity(const Sprite* sprite) const
{
	for (const auto& [entity, component] : sprites_) {
		if (component.sprite == sprite) {
			return entity;
		}
	}
	return kInvalidEntity;
}

void World::PruneMissingBindings(const std::vector<Object3d*>& objects, const std::vector<Sprite*>& sprites)
{
	const std::unordered_set<const Object3d*> objectSet(objects.begin(), objects.end());
	const std::unordered_set<const Sprite*> spriteSet(sprites.begin(), sprites.end());
	std::vector<Entity> removedEntities;
	for (const auto& [entity, model] : models_) {
		if (!objectSet.contains(model.object)) {
			removedEntities.push_back(entity);
		}
	}
	for (const auto& [entity, sprite] : sprites_) {
		if (!spriteSet.contains(sprite.sprite)) {
			removedEntities.push_back(entity);
		}
	}
	for (Entity entity : removedEntities) {
		DestroyEntity(entity);
	}
}

void World::UpdateSystems()
{
	TransformSyncSystem::Update(*this);
	BoxCollisionSystem::Update(*this);
}

bool World::HasModel(Entity entity) const
{
	return models_.contains(entity);
}

bool World::HasSprite(Entity entity) const
{
	return sprites_.contains(entity);
}

bool World::HasBoxCollider(Entity entity) const
{
	return boxColliders_.contains(entity);
}

const NameComponent* World::GetName(Entity entity) const
{
	const auto it = names_.find(entity);
	return it != names_.end() ? &it->second : nullptr;
}

const TransformComponent* World::GetTransform(Entity entity) const
{
	const auto it = transforms_.find(entity);
	return it != transforms_.end() ? &it->second : nullptr;
}

const ModelComponent* World::GetModel(Entity entity) const
{
	const auto it = models_.find(entity);
	return it != models_.end() ? &it->second : nullptr;
}

const SpriteComponent* World::GetSprite(Entity entity) const
{
	const auto it = sprites_.find(entity);
	return it != sprites_.end() ? &it->second : nullptr;
}

BoxColliderComponent* World::GetBoxCollider(Entity entity)
{
	const auto it = boxColliders_.find(entity);
	return it != boxColliders_.end() ? &it->second : nullptr;
}

const BoxColliderComponent* World::GetBoxCollider(Entity entity) const
{
	const auto it = boxColliders_.find(entity);
	return it != boxColliders_.end() ? &it->second : nullptr;
}

BoxColliderComponent& World::AddBoxCollider(Entity entity)
{
	return boxColliders_[entity];
}

void World::RemoveBoxCollider(Entity entity)
{
	boxColliders_.erase(entity);
}

} // namespace Ecs
