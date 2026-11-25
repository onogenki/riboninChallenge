#include "StageGimmickFactory.h"

#include "Object3d.h"
#include "Object3dCollection.h"
#include <array>

namespace
{
	constexpr const char* kSphereModel = "debug/sphere.obj";
	constexpr const char* kWhiteTexture = "resources/debug/white.png";
}

// 木目床の元寸法を従来の床へ合わせ、上面がY=-2.0になる予備位置を設定します。
Engine::Object3d* StageGimmickFactory::CreateFloor(Object3dCollection& objects)
{
	Engine::Object3d* floor = objects.Create("stageObj/floor/woodFloor.obj");
	if (!floor) {
		return nullptr;
	}
	floor->SetScale({ 2.0f, 3.0f, 2.0f });
	floor->SetRotate({ 0.0f, 0.0f, 0.0f });
	floor->SetTranslate({ 0.0f, -3.500001f, 5.0f });
	return floor;
}

// 危険Lightの準備後にSwitchとDoorを作る従来の順番を保ちます。
bool StageGimmickFactory::CreateSwitchesAndDoor(
	Object3dCollection& objects,
	const StageLightPuzzle::Settings& settings,
	SwitchesAndDoor& outObjects)
{
	struct SwitchData
	{
		Vector3 position;
		float radius;
	};
	// 充電Switch、Door Switchの順に、同じ手順で位置と半径を反映します。
	const std::array<SwitchData, 2> switches = {{
		{ settings.chargeSwitchPosition, settings.chargeSwitchRadius },
		{ settings.doorSwitchPosition, settings.doorSwitchRadius },
	}};
	std::array<Engine::Object3d*, 2> created{};
	for (std::size_t i = 0; i < switches.size(); ++i) {
		const SwitchData& data = switches[i];
		Engine::Object3d* switchObject = objects.Create(kSphereModel);
		if (!switchObject) {
			return false;
		}
		switchObject->SetTranslate(data.position);
		const float diameter = data.radius * 2.0f;
		switchObject->SetScale({ diameter, diameter, diameter });
		if (!switchObject->SetTextureOverride(kWhiteTexture)) {
			return false;
		}
		created[i] = switchObject;
	}

	// wall.objを4×3×1に表示し、StageLightPuzzleのColliderと扉の大きさをそろえます。
	Engine::Object3d* lightDoor = objects.Create("stageObj/wall/wall.obj");
	if (!lightDoor) {
		return false;
	}
	lightDoor->SetTranslate(settings.doorClosedPosition);
	lightDoor->SetScale({ 1.0f, 0.75f, 0.7142857f });
	outObjects = { created[0], created[1], lightDoor };
	return true;
}
