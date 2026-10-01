#pragma once

#include "Vector3.h"

class Camera;
class Player;

// Stage1開始時だけ、Playerの落下とCamera移動を行う演出部品です。
class StageStart
{
public:
	// stage1.jsonから読み込む、開始演出の調整値です。
	struct Settings
	{
		// Playerが落下し、Cameraが旋回する時間です。
		float duration = 2.5f;
		// PlayerStartより上へ出現させる高さです。
		float playerAirHeight = 8.0f;
		// 演出開始時に、Playerの前方へ離すCamera距離です。
		float cameraFrontDistance = 10.0f;
		// 演出開始時のCameraの高さです。低くすると見上げる構図になります。
		float cameraFrontHeight = 6.0f;
		// 前方から後方までCameraが回る角度です。半周は約3.14です。
		float cameraOrbitAngle = 3.14159265f;
		// 演出終了後、通常Cameraへつなぐ時間です。
		float cameraHandoffDuration = 0.8f;
	};

	// Stage1が所有するPlayerとCameraを受け取り、開始演出を始めます。
	void Begin(
		Player& player,
		Camera& camera,
		const Vector3& landingPosition,
		const Vector3& gameplayCameraPosition,
		const Vector3& gameplayCameraRotate,
		const Settings& settings);
	// 演出中のPlayerとCameraを更新し、終了した時はfalseを返します。
	bool Update(float deltaTime, Player& player, Camera& camera);
	// Stage1が通常操作を止めるために、演出中かを返します。
	bool IsPlaying() const { return isPlaying_; }
	// 通常Cameraの目標位置へ滑らかに引き継いでいる間だけtrueを返します。
	bool IsCameraHandoffPlaying() const { return isCameraHandoffPlaying_; }
	// CameraControllerが計算した通常Camera位置へ、演出最後のCameraを近づけます。
	void UpdateCameraHandoff(float deltaTime, Camera& camera);

private:
	// 0から1までを、開始と終了でゆっくり変化する値へ変換します。
	float EaseInOut(float t) const;
	// Cameraの位置から注視点を見る回転へ変換します。
	void LookAt(Camera& camera, const Vector3& cameraPosition, const Vector3& targetPosition) const;

	// ---------- 演出の基準座標 ----------

	// Playerが着地する、stage1.jsonのPlayerStart座標です。
	Vector3 landingPosition_{};
	// 演出開始時にPlayerを表示する空中の座標です。
	Vector3 playerAirPosition_{};
	// Playerの正面上側から開始するCamera座標です。
	Vector3 cameraStartPosition_{};
	// 通常の三人称Cameraへ切り替わる直前のCamera座標です。
	Vector3 gameplayCameraPosition_{};
	// 通常の三人称Cameraへ切り替わる直前のCamera回転です。
	Vector3 gameplayCameraRotate_{};
	// ---------- 時間とCamera引き継ぎ ----------

	// 演出の経過時間です。
	float elapsedTime_ = 0.0f;
	// Playerの落下とCamera移動に使う演出時間です。
	float duration_ = 2.5f;
	// Cameraが前方から後方へ回るY軸の角度です。半周はπです。
	float cameraOrbitAngle_ = 3.14159265f;
	// 通常Cameraへ引き継ぐ経過時間です。
	float cameraHandoffElapsedTime_ = 0.0f;
	// 通常Cameraへ引き継ぐ時間です。
	float cameraHandoffDuration_ = 0.8f;
	// 演出が終わった瞬間のCamera位置です。
	Vector3 cameraHandoffStartPosition_{};
	// 演出が終わった瞬間のCamera回転です。
	Vector3 cameraHandoffStartRotate_{};
	// ---------- 演出の状態 ----------

	// trueの間は、Stage1がPlayer入力を止めて開始演出を更新します。
	bool isPlaying_ = false;
	// trueの間は、演出Cameraから通常Cameraへ滑らかに位置を合わせます。
	bool isCameraHandoffPlaying_ = false;
};
