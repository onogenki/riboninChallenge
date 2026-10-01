#pragma once

namespace Ecs {

class World;

class TransformSyncSystem {
public:
	static void Update(World& world);
};

class BoxCollisionSystem {
public:
	static void Update(World& world);
};

} // namespace Ecs
