#pragma once

#include "Vector3.h"

class Player;
class StageLightWarning;

// 危険Light接触後の回数と一秒間のPlayer消失演出を所有します。
// 復帰先とScene切替はStageが決め、この部品はPlayerの消失演出だけを進めます。
class StageLightDeath
{
public:
	// Scene開始またはStage切替時に、前の死亡回数と演出を持ち越さないよう戻します。
	void Reset();
	// 接触を一回だけ記録し、Playerの入力と予兆を死亡演出用の状態へ切り替えます。
	void Begin(Player& player, StageLightWarning& warning);
	// Playerを上昇・透明化し、一秒の演出が終わった時だけtrueを返します。
	bool Update(Player& player, StageLightWarning& warning, float deltaTime);
	// 復帰後に通常更新へ戻します。三回目のScene切替待ちでは呼びません。
	void CompleteRespawn();

	// StageがPlayer・Cameraの通常更新を止めるため、演出中かを返します。
	bool IsPlaying() const { return isPlaying_; }
	// 三回目だけGame Overを選ぶため、現在Stageでの接触回数を返します。
	int GetDeathCount() const { return deathCount_; }
	// 復帰Cameraの相対位置を保つため、接触時のPlayer座標を返します。
	const Vector3& GetStartPosition() const { return startPosition_; }

private:
	int deathCount_ = 0;
	bool isPlaying_ = false;
	float elapsedTime_ = 0.0f;
	Vector3 startPosition_{};
};
