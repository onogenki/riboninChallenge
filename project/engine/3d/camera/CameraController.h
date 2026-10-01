#pragma once

#include "MyMath.h"
#include <vector>

class Camera;

// 狭い通路やボス部屋など、場所ごとに変更するCameraの基本設定
struct CameraAreaSettings
{
	float distance = 14.0f;
	float pitch = 0.32f;
	float fovY = 0.60f;
};

// Player や敵などの対象を、少し遅れて追いかける三人称カメラ用の操作クラス
class CameraController
{
public:
	// 操作する Camera と、最初に追いかける対象の位置を登録する
	void Initialize(Camera* camera, const Vector3& targetPosition);
	// Camera配置済みのイベント視点を、指定した周回値から手動Cameraとして開始する
	void Initialize(
		Camera* camera,
		const Vector3& targetPosition,
		float distance,
		float orbitYaw,
		float orbitPitch);
	// 対象の現在位置と移動方向を受け取り、Focus と Camera を更新する
	void Update(
		float deltaTime,
		const Vector3& targetPosition,
		const Vector3& targetMoveDirection,
		bool isOrbitInput,
		const std::vector<MyMath::OBB>& cameraCollisionObbs);

	// カメラの高さ・距離・横方向の角度を設定する
	void SetDistance(float distance);
	void SetOrbitYaw(float orbitYaw);
	// マウス操作で、カメラをFocusの周囲へ回す角度を加算する
	void AddOrbitYaw(float deltaYaw);
	void AddOrbitPitch(float deltaPitch);
	// 左右キー一回につき一段階だけ周回し、壁へ近づく方向なら変更しません。
	bool TryStepOrbit(
		int stepDirection,
		const std::vector<MyMath::OBB>& cameraCollisionObbs);
	// 上下キー一回につき一段階だけ遠近を変え、壁へ入る距離なら変更しません。
	bool TryStepDistance(
		int stepDirection,
		const std::vector<MyMath::OBB>& cameraCollisionObbs);
	// Playerが向いている方向の後ろへ、カメラをゆっくり戻す
	void ResetBehindTarget(float targetFacingYaw);
	// 開始演出などで動かしたCamera位置を、通常追従Cameraの現在値として引き継ぎます。
	void SynchronizeToCamera(const Vector3& targetPosition);
	// Areaへ入った時だけ、Cameraの距離・縦角度・視野角を変更する
	void SetAreaSettings(const CameraAreaSettings& settings);
	void ClearAreaSettings();
	// 追従対象の中心から、カメラが見る Focus までのずれを設定する
	void SetFocusOffset(const Vector3& offset) { focusOffset_ = offset; }
	// Player がこの半径内にいる間は Focus を動かさない
	void SetDeadZoneRadius(float radius) { deadZoneRadius_ = radius; }
	// Focus と Camera が追従する速さを設定する
	void SetFocusFollowSpeed(float speed) { focusFollowSpeed_ = speed; }
	void SetCameraFollowSpeed(float speed) { cameraFollowSpeed_ = speed; }
	// falseにすると、手動Cameraで移動中も背後へ自動整列しない
	void SetAutoRecenterEnabled(bool enabled) { isAutoRecenterEnabled_ = enabled; }
	// falseにすると、壁がCameraとPlayerの間にあってもCamera位置をPlayer側へ縮めません。
	// これはCamera位置だけの設定です。壁越し表示はObject側の明示的な描画呼び出しが必要です。
	void SetWallAvoidanceEnabled(bool enabled) { isWallAvoidanceEnabled_ = enabled; }
	// 持ち物を構えている間など、通常Cameraが設定距離より近付かないようにします。
	// 壁回避による一時的な接近は安全のため許可します。
	void SetDistanceLock(bool locked);

	float GetDistance() const { return distance_; }
	float GetOrbitYaw() const { return orbitYaw_; }
	// 補間中でも、指定した通常Cameraの目標角度を編集画面へ表示します。
	float GetTargetOrbitYaw() const { return targetOrbitYaw_; }
	float GetOrbitPitch() const { return orbitPitch_; }
	int GetOrbitStepIndex() const { return orbitStepIndex_; }
	int GetDistanceStepIndex() const { return distanceStepIndex_; }
	const Vector3& GetFocus() const { return focus_; }
	const Vector3& GetCameraPosition() const { return cameraPosition_; }

private:
	// 対象の位置・進行方向から、Cameraが見るFocusとLookAtを追従更新します。
	bool UpdateFocus(float deltaTime, const Vector3& targetPosition, const Vector3& targetMoveDirection);
	// 距離・周回角度・自動背後戻しを更新します。
	void UpdateOrbit(float deltaTime, const Vector3& targetMoveDirection, bool isTargetMoving, bool isOrbitInput);
	// 壁回避済みCamera位置と、移動中の視野角を更新してCameraへ反映します。
	void UpdateCameraTransform(
		float deltaTime,
		bool isTargetMoving,
		const std::vector<MyMath::OBB>& cameraCollisionObbs);
	// Focus の後ろ・上にある、カメラが目指す位置を計算する
	Vector3 CalculateTargetCameraPosition() const;
	Vector3 CalculateTargetCameraPosition(float yaw, float pitch, float distance) const;
	// 指定候補までの経路で、壁に遮られず使用できる距離の割合を返します。
	float CalculateAvailablePathRatio(
		float yaw,
		float pitch,
		float distance,
		const std::vector<MyMath::OBB>& cameraCollisionObbs) const;
	bool IsCameraSettingAvailable(
		float yaw,
		float pitch,
		float distance,
		const std::vector<MyMath::OBB>& cameraCollisionObbs) const;
	// Focusから理想位置までの線が壁に当たるなら、壁の手前の安全な位置を返す
	Vector3 CalculateCollisionSafeCameraPosition(
		const std::vector<MyMath::OBB>& cameraCollisionObbs) const;
	// 補間途中でもCameraが外壁を越えないよう、現在位置を壁の内側へ収めます。
	Vector3 ClampCameraPositionToCollisionBoundaries(
		const Vector3& cameraPosition,
		const std::vector<MyMath::OBB>& cameraCollisionObbs) const;
	// 現在のカメラ位置から Focus を向く回転を計算して Camera に設定する
	void ApplyCameraTransform();
	// 角度が -π と +π をまたいでも、短い方向へ補間する
	float LerpAngle(float current, float target, float t) const;
	float MoveTowardsAngle(float current, float target, float maxDelta) const;

	// 実際に移動させる共通 Camera。本クラスは所有しない
	Camera* camera_ = nullptr;
	// Camera が見続ける中心点
	Vector3 focus_{};
	// Focusより少し遅れてCameraが見る、視線専用の注視点
	Vector3 lookAt_{};
	// カメラ自身の現在位置
	Vector3 cameraPosition_{};
	// Player の中心より少し上を見続けるためのずれ
	Vector3 focusOffset_{ 0.0f, 1.2f, 0.0f };
	// Focusとの直線距離。ホイールで変更する
	float distance_ = 14.0f;
	// Areaの外でプレイヤーがホイール・縦ドラッグにより変更した設定
	float manualDistance_ = 14.0f;
	float manualOrbitPitch_ = 0.32f;
	// 現在値がゆっくり近づく、距離と縦角度の目標値
	float targetDistance_ = 14.0f;
	// 鏡を持った時に確保した、通常Cameraの最小距離です。
	// 鏡を離した後も維持し、Playerが矢印キーで近付けた時だけ小さくなります。
	float lockedMinimumDistance_ = 0.0f;
	// trueの間、鏡を持っている最中であることを表します。
	bool isDistanceLocked_ = false;
	// Player の向きとは独立した、カメラ配置用の Y 軸角度
	float orbitYaw_ = 0.0f;
	float targetOrbitYaw_ = 0.0f;
	// 左右段階の中心となる角度です。RキーでPlayer後方へ更新します。
	float orbitAnchorYaw_ = 0.0f;
	// 左右キーを押した回数です。上限を設けず、一周以上の周回にも使います。
	int orbitStepIndex_ = 0;
	float orbitStepAngle_ = 0.52359878f;
	// Focusの周囲を上下へ回すX軸角度
	float orbitPitch_ = 0.32f;
	float targetOrbitPitch_ = 0.32f;
	float minimumOrbitPitch_ = -0.15f;
	float maximumOrbitPitch_ = 1.15f;
	// Player が Focus の周辺にいる間、Focus を止める半径
	float deadZoneRadius_ = 0.45f;
	// Focus と Camera の追従速度
	float focusFollowSpeed_ = 7.0f;
	float lookAtFollowSpeed_ = 5.5f;
	float cameraFollowSpeed_ = 4.5f;
	// 外壁へ近付いた時だけ、急に寄らずゆっくりCameraを縮める速さです。
	float wallAvoidanceFollowSpeed_ = 3.0f;
	// 矢印キーで選んだ方向へ、ロボットが向きを変えるよう素早く移動します。
	float orbitFollowSpeed_ = 24.0f;
	// Playerが移動している方向の少し先へFocusをずらす距離
	float lookAheadDistance_ = 0.45f;
	// 走り続けた時に、自動でPlayerの後ろへ戻り始めるまでの時間
	float autoRecenterDelay_ = 0.8f;
	float autoRecenterTimer_ = 0.0f;
	float autoRecenterSpeed_ = 2.0f;
	bool isAutoRecenterEnabled_ = true;
	// trueの時だけ、CameraとFocusの間にある壁の手前へCameraを移動します。
	bool isWallAvoidanceEnabled_ = true;
	// 上下キーで選ぶ近・中・遠の三段階です。
	float minimumManualDistance_ = 10.0f;
	float manualDistanceStep_ = 2.0f;
	int distanceStepIndex_ = 2;
	int maximumDistanceStep_ = 3;
	// 候補位置までの経路がこの割合より短くなる場合、壁側への操作を拒否します。
	float minimumAvailablePathRatio_ = 0.82f;
	// 走行中だけ少し広くする、視野角の値と追従速度
	float baseFovY_ = 0.60f;
	float movingFovY_ = 0.64f;
	float currentFovY_ = 0.60f;
	float fovFollowSpeed_ = 2.0f;
	// Areaに入っている間だけ有効にする、Camera基本設定
	bool hasAreaSettings_ = false;
	float areaBaseFovY_ = 0.60f;
	// Cameraを小さな球として扱い、壁の近くで少し手前へ止めるための値
	float cameraCollisionRadius_ = 0.3f;
	float cameraCollisionMargin_ = 0.1f;
};
