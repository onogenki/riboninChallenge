#pragma once

#include "Vector3.h"

class Player;
class StageHazardLights;
class StageLightPuzzle;

// 反射後の危険LightとPlayerの距離を調べ、光が来る方向の予兆だけを滑らかに更新します。
class StageLightWarning
{
public:
	// AABBで候補を絞った後、最短距離からPlayer個体の予兆方向と強さを更新します。
	void Update(
		Player& player,
		const StageLightPuzzle& lightPuzzle,
		const StageHazardLights& hazardLights,
		float deltaTime);
	// 接触直前の方向と強さを保存し、死亡演出へ途切れず引き継ぎます。
	void BeginDeath();
	// 接触時の方向を保ち、消失まで反射範囲と強さを広げます。
	void UpdateDeath(Player& player, float progress);
	// 別Stageやリスポーンで、Playerの予兆だけを通常状態へ戻します。
	void Reset(Player& player);

private:
	Vector3 direction_{ 0.0f, 0.0f, 1.0f };
	float strength_ = 0.0f;
	float deathStartStrength_ = 0.0f;
};
