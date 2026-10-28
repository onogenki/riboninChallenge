#include "StageLightDeath.h"

#include "Player.h"
#include "StageLightWarning.h"
#include <algorithm>

// 別Stageの死亡回数や途中の透明化を、新しいStageへ引き継がないようにします。
void StageLightDeath::Reset()
{
	deathCount_ = 0;
	isPlaying_ = false;
	elapsedTime_ = 0.0f;
	startPosition_ = {};
}

// 接触した位置と回数を確定してから、予兆とPlayerの状態を消失演出へ渡します。
void StageLightDeath::Begin(Player& player, StageLightWarning& warning)
{
	if (isPlaying_) {
		return;
	}
	++deathCount_;
	isPlaying_ = true;
	elapsedTime_ = 0.0f;
	startPosition_ = player.GetPosition();
	warning.BeginDeath();
	player.SetMirrorGuardMode(false);
}

// 従来と同じ一秒の間に上昇・透明化・反射光を更新し、復帰判断はStageへ返します。
bool StageLightDeath::Update(Player& player, StageLightWarning& warning, float deltaTime)
{
	if (!isPlaying_) {
		return false;
	}
	constexpr float kDeathDuration = 1.0f;
	constexpr float kFadeDuration = 0.4f;
	constexpr float kRiseDuration = 0.55f;
	constexpr float kDeathRiseHeight = 4.0f;
	elapsedTime_ = (std::min)(elapsedTime_ + deltaTime, kDeathDuration);
	const float progress = elapsedTime_ / kDeathDuration;
	const float riseProgress = (std::min)(elapsedTime_ / kRiseDuration, 1.0f);
	const float fadeProgress = (std::min)(elapsedTime_ / kFadeDuration, 1.0f);
	player.SetPosition({
		startPosition_.x,
		startPosition_.y + kDeathRiseHeight * riseProgress,
		startPosition_.z,
	});
	player.GetObject().SetOpacity(1.0f - fadeProgress);
	// 透明化と同時に鏡面反射を強め、青白い光は消え切る前だけ最大になります。
	player.GetObject().SetSpecularGlow(
		1.0f + 10.0f * fadeProgress,
		12.0f * fadeProgress * (1.0f - fadeProgress));
	warning.UpdateDeath(player, fadeProgress);
	return progress >= 1.0f;
}

// StageがPlayerとCameraを復帰させた後だけ、次の接触を受けられるようにします。
void StageLightDeath::CompleteRespawn()
{
	isPlaying_ = false;
}
