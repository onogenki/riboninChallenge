#pragma once

#include "MyMath.h"
#include "Transform.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Engine { class Object3d; }
namespace Engine { class Sprite; }

namespace Ecs {

// Entity is a stable numeric ID. It does not contain game behavior by itself.
using Entity = uint32_t;
inline constexpr Entity kInvalidEntity = 0;

struct NameComponent {
	// Editor-facing entity name.
	std::string value;
};

struct TransformComponent {
	// Cached transform synchronized from the attached render object.
	Transform value{};
};

struct ModelComponent {
	// Non-owning pointer; DebugScene owns the Object3d instance.
	Engine::Object3d* object = nullptr;
	std::string sourceFile;
	bool isAnimated = false;
};

struct SpriteComponent {
	// Non-owning pointer; DebugScene owns the Sprite instance.
	Engine::Sprite* sprite = nullptr;
	std::string sourceFile;
};

// BoxCollider is an axis-aligned box used for simple overlap checks.
struct BoxColliderComponent {
	Vector3 center{};
	Vector3 halfExtents{ 0.5f, 0.5f, 0.5f };
	MyMath::AABB worldBounds{};
	bool enabled = true;
	bool isTrigger = false;
	bool isColliding = false;
};

class TransformSyncSystem;
class BoxCollisionSystem;

// DebugSceneのEntityとComponentを保持し、描画物への参照を対応付けます。
class World {
public:
	// Creates an entity and its mandatory name and transform components.
	Entity CreateEntity(const std::string& name);
	// Removes every component owned by the entity.
	void DestroyEntity(Entity entity);
	// Clears all entities and resets the numeric ID generator.
	void Clear();

	bool IsAlive(Entity entity) const;
	const std::vector<Entity>& GetEntities() const { return entities_; }

	// Creates an entity linked to a scene-owned render object.
	Entity CreateModelEntity(Engine::Object3d* object, const std::string& sourceFile, bool isAnimated);
	Entity CreateSpriteEntity(Engine::Sprite* sprite, const std::string& sourceFile);
	Entity FindEntity(const Engine::Object3d* object) const;
	Entity FindEntity(const Engine::Sprite* sprite) const;

	// Drops ECS entities whose scene-owned Object3d or Sprite has been deleted.
	void PruneMissingBindings(const std::vector<Engine::Object3d*>& objects, const std::vector<Engine::Sprite*>& sprites);
	// Synchronizes transforms and calculates collider overlaps for the current frame.
	void UpdateSystems();

	bool HasModel(Entity entity) const;
	bool HasSprite(Entity entity) const;
	bool HasBoxCollider(Entity entity) const;
	const NameComponent* GetName(Entity entity) const;
	const TransformComponent* GetTransform(Entity entity) const;
	const ModelComponent* GetModel(Entity entity) const;
	const SpriteComponent* GetSprite(Entity entity) const;
	BoxColliderComponent* GetBoxCollider(Entity entity);
	const BoxColliderComponent* GetBoxCollider(Entity entity) const;
	// Adds a default box collider if the entity does not already have one.
	BoxColliderComponent& AddBoxCollider(Entity entity);
	void RemoveBoxCollider(Entity entity);

private:
	Entity nextEntity_ = 1;
	std::vector<Entity> entities_;
	std::unordered_set<Entity> aliveEntities_;
	std::unordered_map<Entity, NameComponent> names_;
	std::unordered_map<Entity, TransformComponent> transforms_;
	std::unordered_map<Entity, ModelComponent> models_;
	std::unordered_map<Entity, SpriteComponent> sprites_;
	std::unordered_map<Entity, BoxColliderComponent> boxColliders_;

	friend class TransformSyncSystem;
	friend class BoxCollisionSystem;
};

} // namespace Ecs
