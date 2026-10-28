#pragma once

#include "StageLightPuzzle.h"

class Object3d;
class Object3dCollection;

// Stage固有の通常3Dギミックを生成します。寿命は呼出元のCollectionが所有します。
class StageGimmickFactory
{
public:
	struct LaserEmitters
	{
		Object3d* charge = nullptr;
		Object3d* door = nullptr;
	};

	struct SwitchesAndDoor
	{
		Object3d* chargeSwitch = nullptr;
		Object3d* doorSwitch = nullptr;
		Object3d* lightDoor = nullptr;
	};

	// JSON反映までの予備位置に床を置き、生成できない場合はnullptrを返します。
	static Object3d* CreateFloor(Object3dCollection& objects);
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
