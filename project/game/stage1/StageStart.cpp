#include "StageStart.h"

#include "Camera.h"
#include "Player.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

void StageStart::Begin(
	Player& player,
	Camera& camera,
	const Vector3& landingPosition,
	const Vector3& gameplayCameraPosition,
	const Vector3& gameplayCameraRotate,
	const Settings& settings)
{
	// Stage1が決めた通常Cameraの完成位置を保存し、演出中は直接書き換えません。
	landingPosition_ = landingPosition;
	gameplayCameraPosition_ = gameplayCameraPosition;
	gameplayCameraRotate_ = gameplayCameraRotate;

	// 着地点の真上から、ゆっくり落下してくるように開始位置を作ります。
	playerAirPosition_ = landingPosition_;
	playerAirPosition_.y += settings.playerAirHeight;

	// +Z側かつ上側はPlayerの正面上方です。通常Cameraは-Z側の背後へ移動します。
	cameraStartPosition_ = landingPosition_;
	cameraStartPosition_.y += settings.cameraFrontHeight;
	cameraStartPosition_.z += settings.cameraFrontDistance;

	elapsedTime_ = 0.0f;
	duration_ = (std::max)(settings.duration, 0.1f);
	cameraOrbitAngle_ = settings.cameraOrbitAngle;
	// JSONの引き継ぎ時間を使い、演出終了時に通常Cameraへ急に切り替わらないようにします。
	cameraHandoffDuration_ = (std::max)(settings.cameraHandoffDuration, 0.1f);
	isPlaying_ = true;
	isCameraHandoffPlaying_ = false;
	player.SetPosition(playerAirPosition_);
	camera.SetTranslate(cameraStartPosition_);
	LookAt(camera, cameraStartPosition_, playerAirPosition_);
}

bool StageStart::Update(float deltaTime, Player& player, Camera& camera)
{
	if (!isPlaying_) {
		return false;
	}

	// 経過時間を0〜1へ正規化します。0は開始、1は演出終了です。
	elapsedTime_ += deltaTime;
	const float progress = std::clamp(elapsedTime_ / duration_, 0.0f, 1.0f);
	// EaseInOutを通すと、開始・終了だけ遅く、途中は自然に速くなります。
	const float easedProgress = EaseInOut(progress);

	// 空中座標から着地点へ、各軸を同じ割合で補間してPlayerを落下させます。
	const Vector3 playerPosition{
		playerAirPosition_.x + (landingPosition_.x - playerAirPosition_.x) * easedProgress,
		playerAirPosition_.y + (landingPosition_.y - playerAirPosition_.y) * easedProgress,
		playerAirPosition_.z + (landingPosition_.z - playerAirPosition_.z) * easedProgress,
	};
	const Vector3 startOffset{
		cameraStartPosition_.x - landingPosition_.x,
		0.0f,
		cameraStartPosition_.z - landingPosition_.z,
	};
	const Vector3 endOffset{
		gameplayCameraPosition_.x - landingPosition_.x,
		0.0f,
		gameplayCameraPosition_.z - landingPosition_.z,
	};
	// CameraをPlayerの周囲で回すため、XZ平面の位置を「半径」と「角度」へ分けます。
	const float startRadius = std::sqrt(startOffset.x * startOffset.x + startOffset.z * startOffset.z);
	const float endRadius = std::sqrt(endOffset.x * endOffset.x + endOffset.z * endOffset.z);
	const float radius = startRadius + (endRadius - startRadius) * easedProgress;
	const float startAngle = std::atan2(startOffset.x, startOffset.z);
	const float angle = startAngle + cameraOrbitAngle_ * easedProgress;
	// 開始演出は部屋の天井より外へ出ないよう、開始時の室内高さを保ちます。
	// 通常Cameraの高い見下ろし位置へは、演出後に壁回避済みの位置へ滑らかに引き継ぎます。
	// sin/cosで半径と角度を再びXZ座標へ戻すと、Playerを中心とする円弧移動になります。
	const Vector3 cameraPosition{
		landingPosition_.x + std::sin(angle) * radius,
		cameraStartPosition_.y,
		landingPosition_.z + std::cos(angle) * radius,
	};

	player.SetPosition(playerPosition);
	camera.SetTranslate(cameraPosition);
	LookAt(camera, cameraPosition, playerPosition);

	if (progress < 1.0f) {
		return true;
	}

	// 演出の最終位置を保存し、次のフレームから通常Cameraの安全な目標位置へ近づけます。
	player.SetPosition(landingPosition_);
	cameraHandoffStartPosition_ = camera.GetTranslate();
	cameraHandoffStartRotate_ = camera.GetRotate();
	cameraHandoffElapsedTime_ = 0.0f;
	isPlaying_ = false;
	isCameraHandoffPlaying_ = true;
	return false;
}

void StageStart::UpdateCameraHandoff(float deltaTime, Camera& camera)
{
	if (!isCameraHandoffPlaying_) {
		return;
	}

	// CameraControllerが先に壁回避・Player追従を反映した、通常Cameraの目標値を取得します。
	const Vector3 gameplayTargetPosition = camera.GetTranslate();
	const Vector3 gameplayTargetRotate = camera.GetRotate();
	cameraHandoffElapsedTime_ += deltaTime;
	const float progress = std::clamp(
		cameraHandoffElapsedTime_ / cameraHandoffDuration_,
		0.0f,
		1.0f);
	const float easedProgress = EaseInOut(progress);

	// CameraControllerの安全な目標へ補間し、演出終了フレームの急な位置切替をなくします。
	camera.SetTranslate({
		cameraHandoffStartPosition_.x +
			(gameplayTargetPosition.x - cameraHandoffStartPosition_.x) * easedProgress,
		cameraHandoffStartPosition_.y +
			(gameplayTargetPosition.y - cameraHandoffStartPosition_.y) * easedProgress,
		cameraHandoffStartPosition_.z +
			(gameplayTargetPosition.z - cameraHandoffStartPosition_.z) * easedProgress,
	});
	camera.SetRotate({
		cameraHandoffStartRotate_.x +
			(gameplayTargetRotate.x - cameraHandoffStartRotate_.x) * easedProgress,
		cameraHandoffStartRotate_.y +
			(gameplayTargetRotate.y - cameraHandoffStartRotate_.y) * easedProgress,
		cameraHandoffStartRotate_.z +
			(gameplayTargetRotate.z - cameraHandoffStartRotate_.z) * easedProgress,
	});

	if (progress >= 1.0f) {
		isCameraHandoffPlaying_ = false;
	}
}

float StageStart::EaseInOut(float t) const
{
	// smoothstep式です。t=0なら0、t=1なら1になり、両端で速度が0へ近づきます。
	const float clampedT = std::clamp(t, 0.0f, 1.0f);
	return clampedT * clampedT * (3.0f - 2.0f * clampedT);
}

void StageStart::LookAt(
	Camera& camera,
	const Vector3& cameraPosition,
	const Vector3& targetPosition) const
{
	const Vector3 lookDirection = Normalize({
		targetPosition.x - cameraPosition.x,
		targetPosition.y - cameraPosition.y,
		targetPosition.z - cameraPosition.z,
	});
	// 正規化済み方向ベクトルから、上下角pitchと左右角yawへ変換します。
	// asinへ渡す値を-1〜1へ制限し、丸め誤差で計算不能になることを防ぎます。
	const float pitch = -std::asin(std::clamp(lookDirection.y, -1.0f, 1.0f));
	const float yaw = std::atan2(lookDirection.x, lookDirection.z);
	camera.SetRotate({ pitch, yaw, 0.0f });
}
