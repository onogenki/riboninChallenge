#pragma once

#include "StageLightPuzzle.h"

namespace Engine { class Object3d; }
class Object3dCollection;

// Stage固有の通常3Dギミックを生成します。寿命は呼出元のCollectionが所有します。
class StageGimmickFactory
{
public:
	struct LaserEmitters
	{
		Engine::Object3d* charge = nullptr;
		Engine::Object3d* door = nullptr;
	};

	struct SwitchesAndDoor
	{
		Engine::Object3d* chargeSwitch = nullptr;
		Engine::Object3d* doorSwitch = nullptr;
		Engine::Object3d* lightDoor = nullptr;
	};

	// JSON反映までの予備位置に床を置き、生成できない場合はnullptrを返します。
	static Engine::Object3d* CreateFloor(Object3dCollection& objects);
	// 二つのLaser発射球を設定値の位置へ置き、必須画像がなければ失敗を返します。
	static bool CreateLaserEmitters(
		Object3dCollection& objects,
		const StageLightPuzzle::Settings& settings,
		LaserEmitters& outEmitters);
	// Switch二つとDoorを作り、必須画像がなければ失敗を返します。
	static bool CreateSwitchesAndDoor(
		Object3dCollection& objects,
		const StageLightPuzzle::Settings& settings,
		SwitchesAndDoor& outObjects);
};
