#include "StageGimmickFactory.h"

#include "Object3d.h"
#include "Object3dCollection.h"

// 床の上面が従来どおりY=-2.0になる予備位置を設定します。
Object3d* StageGimmickFactory::CreateFloor(Object3dCollection& objects)
{
	Object3d* floor = objects.Create("debug/floor.obj");
	if (!floor) {
		return nullptr;
	}
	floor->SetScale({ 1.0f, 1.0f, 1.0f });
	floor->SetRotate({ 0.0f, 0.0f, 0.0f });
	floor->SetTranslate({ 0.0f, -3.500001f, 5.0f });
	return floor;
}

// 光の出発点を示す球を従来と同じ順に作り、画像欠損を成功扱いしません。
bool StageGimmickFactory::CreateLaserEmitters(
	Object3dCollection& objects,
	const StageLightPuzzle::Settings& settings,
	LaserEmitters& outEmitters)
{
	Object3d* charge = objects.Create("debug/sphere.obj");
	if (!charge) {
		return false;
	}
	charge->SetTranslate(settings.laserOrigin);
	charge->SetScale({ 0.9f, 0.9f, 0.9f });
	if (!charge->SetTextureOverride("resources/debug/white.png")) {
		return false;
	}

	Object3d* door = objects.Create("debug/sphere.obj");
	if (!door) {
		return false;
	}
	door->SetTranslate(settings.doorLaserOrigin);
	door->SetScale({ 0.65f, 0.65f, 0.65f });
	if (!door->SetTextureOverride("resources/debug/white.png")) {
		return false;
	}
	outEmitters = { charge, door };
	return true;
}

// 危険Lightの準備後にSwitchとDoorを作る従来の順番を保ちます。
bool StageGimmickFactory::CreateSwitchesAndDoor(
	Object3dCollection& objects,
	const StageLightPuzzle::Settings& settings,
	SwitchesAndDoor& outObjects)
{
	Object3d* chargeSwitch = objects.Create("debug/sphere.obj");
	if (!chargeSwitch) {
		return false;
	}
	chargeSwitch->SetTranslate(settings.chargeSwitchPosition);
	chargeSwitch->SetScale({
		settings.chargeSwitchRadius * 2.0f,
		settings.chargeSwitchRadius * 2.0f,
		settings.chargeSwitchRadius * 2.0f,
	});
	if (!chargeSwitch->SetTextureOverride("resources/debug/white.png")) {
		return false;
	}

	Object3d* doorSwitch = objects.Create("debug/sphere.obj");
	if (!doorSwitch) {
		return false;
	}
	doorSwitch->SetTranslate(settings.doorSwitchPosition);
	doorSwitch->SetScale({
		settings.doorSwitchRadius * 2.0f,
		settings.doorSwitchRadius * 2.0f,
		settings.doorSwitchRadius * 2.0f,
	});
	if (!doorSwitch->SetTextureOverride("resources/debug/white.png")) {
		return false;
	}

	Object3d* lightDoor = objects.Create("debug/floor.obj");
	if (!lightDoor) {
		return false;
	}
	lightDoor->SetTranslate(settings.doorClosedPosition);
	lightDoor->SetScale({ 0.20f, 1.00f, 0.05f });
	outObjects = { chargeSwitch, doorSwitch, lightDoor };
	return true;
}
