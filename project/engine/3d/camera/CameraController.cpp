#include "CameraController.h"

#include "Camera.h"
#include "Collision.h"
#include "MyMath.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace MyMath;

namespace Engine
{
namespace
{
	// 通常視点を遮る壁が近付いた時だけ上昇し、天井の手前までの距離も候補ごとに調べます。
	constexpr float kWallAvoidanceStartDistance = 8.0f;
	constexpr float kWallAvoidanceMaximumPitch = 1.45f;
	constexpr int kWallAvoidancePitchSamples = 16;
	constexpr float kWallAvoidanceRiseSpeed = 7.0f;
	constexpr float kWallAvoidanceReturnSpeed = 3.0f;
	constexpr float kWallAvoidancePositionSpeed = 6.0f;
	constexpr float kWallAvoidanceTurnSpeed = 2.5f;
	constexpr float kWallAvoidanceYawStep = 0.15f;
	constexpr int kWallAvoidanceYawSamples = 8;
}

void CameraController::Initialize(Engine::Camera* camera, const Vector3& targetPosition)
{
	// Controller は Camera を作らず、Scene から渡された Camera を操作する
	camera_ = camera;
	focus_ = {
		targetPosition.x + focusOffset_.x,
		targetPosition.y + focusOffset_.y,
		targetPosition.z + focusOffset_.z,
	};
	lookAt_ = focus_;
	currentFovY_ = baseFovY_;
	targetDistance_ = distance_;
	manualDistance_ = distance_;
	manualOrbitPitch_ = orbitPitch_;
	wallPitchOffset_ = 0.0f;
	wallYawOffset_ = 0.0f;
	isAutomaticWallViewEnabled_ = true;
	orbitAnchorYaw_ = targetOrbitYaw_;
	orbitStepIndex_ = 0;
	distanceStepIndex_ = std::clamp(
		static_cast<int>(std::round((manualDistance_ - minimumManualDistance_) / manualDistanceStep_)),
		0,
		maximumDistanceStep_);
	cameraPosition_ = CalculateTargetCameraPosition();
	ApplyCameraTransform();
}

void CameraController::Initialize(
	Engine::Camera* camera,
	const Vector3& targetPosition,
	float distance,
	float orbitYaw,
	float orbitPitch)
{
	// イベントCameraがJSONで持つ位置を、Player基準の周回値へ変換した結果で開始する
	camera_ = camera;
	distance_ = (std::max)(distance, 0.1f);
	manualDistance_ = distance_;
	targetDistance_ = distance_;
	orbitYaw_ = orbitYaw;
	targetOrbitYaw_ = orbitYaw;
	orbitAnchorYaw_ = orbitYaw;
	orbitStepIndex_ = 0;
	orbitPitch_ = std::clamp(orbitPitch, minimumOrbitPitch_, maximumOrbitPitch_);
	manualOrbitPitch_ = orbitPitch_;
	wallPitchOffset_ = 0.0f;
	wallYawOffset_ = 0.0f;
	isAutomaticWallViewEnabled_ = false;
	targetOrbitPitch_ = orbitPitch_;
	distanceStepIndex_ = std::clamp(
		static_cast<int>(std::round((manualDistance_ - minimumManualDistance_) / manualDistanceStep_)),
		0,
		maximumDistanceStep_);
	focus_ = {
		targetPosition.x + focusOffset_.x,
		targetPosition.y + focusOffset_.y,
		targetPosition.z + focusOffset_.z,
	};
	lookAt_ = focus_;
	cameraPosition_ = CalculateTargetCameraPosition();
	ApplyCameraTransform();
}

void CameraController::Update(
	float deltaTime,
	const Vector3& targetPosition,
	const Vector3& targetMoveDirection,
	bool isOrbitInput,
	const std::vector<MyMath::OBB>& cameraCollisionObbs)
{
	if (!camera_) {
		return;
	}

	const bool isTargetMoving = UpdateFocus(deltaTime, targetPosition, targetMoveDirection);
	UpdateOrbit(deltaTime, targetMoveDirection, isTargetMoving, isOrbitInput);
	UpdateCameraTransform(deltaTime, isTargetMoving, cameraCollisionObbs);
}

// 対象の位置と進行方向から、少し遅れて追従するFocusと視線を更新します。
bool CameraController::UpdateFocus(
	float deltaTime,
	const Vector3& targetPosition,
	const Vector3& targetMoveDirection)
{
	const bool isTargetMoving = Length(targetMoveDirection) > 0.0001f;
	Vector3 lookAhead{};
	if (isTargetMoving) {
		const Vector3 moveDirection = Normalize(targetMoveDirection);
		lookAhead = {
			moveDirection.x * lookAheadDistance_,
			0.0f,
			moveDirection.z * lookAheadDistance_,
		};
	}

	// Player の中心より少し上かつ進行方向の先を、Focus の目標にする
	const Vector3 targetFocus{
		targetPosition.x + focusOffset_.x + lookAhead.x,
		targetPosition.y + focusOffset_.y,
		targetPosition.z + focusOffset_.z + lookAhead.z,
	};
	const Vector3 focusToTarget{
		targetFocus.x - focus_.x,
		targetFocus.y - focus_.y,
		targetFocus.z - focus_.z,
	};
	const float focusDistance = Length(focusToTarget);
	Vector3 desiredFocus = focus_;

	// 対象が Dead Zone の外へ出た時だけ、Focus の目標位置を作る
	if (focusDistance > deadZoneRadius_) {
		const Vector3 direction = Normalize(focusToTarget);
		desiredFocus = {
			targetFocus.x - direction.x * deadZoneRadius_,
			targetFocus.y - direction.y * deadZoneRadius_,
			targetFocus.z - direction.z * deadZoneRadius_,
		};
	}

	// フレームレートに左右されにくい割合で Focus をゆっくり移動させる
	const float focusT = 1.0f - std::exp(-focusFollowSpeed_ * deltaTime);
	focus_ = Lerp(focus_, desiredFocus, focusT);
	// 視線も別に補間し、Playerが急に曲がっても画面の向きを急変させない
	const float lookAtT = 1.0f - std::exp(-lookAtFollowSpeed_ * deltaTime);
	lookAt_ = Lerp(lookAt_, focus_, lookAtT);
	return isTargetMoving;
}

// 距離・周回角度と、操作していない時の自動背後戻しを更新します。
void CameraController::UpdateOrbit(
	float deltaTime,
	const Vector3& targetMoveDirection,
	bool isTargetMoving,
	bool isOrbitInput)
{
	const float orbitT = 1.0f - std::exp(-orbitFollowSpeed_ * deltaTime);
	// 鏡を離した次のFrameにCameraAreaが近い距離を再設定しても、
	// 鏡を持った時に確保した見やすい距離より近付かないようにします。
	const float effectiveTargetDistance = (std::max)(targetDistance_, lockedMinimumDistance_);
	distance_ += (effectiveTargetDistance - distance_) * orbitT;

	// Cameraを操作していない状態で走り続けると、少しずつPlayerの後ろへ戻る
	if (isAutoRecenterEnabled_ && isTargetMoving && !isOrbitInput) {
		autoRecenterTimer_ += deltaTime;
		if (autoRecenterTimer_ >= autoRecenterDelay_) {
			const float moveYaw = std::atan2(targetMoveDirection.x, targetMoveDirection.z);
			targetOrbitYaw_ = MoveTowardsAngle(
				targetOrbitYaw_,
				moveYaw,
				autoRecenterSpeed_ * deltaTime);
			orbitAnchorYaw_ = targetOrbitYaw_;
			orbitStepIndex_ = 0;
		}
	} else {
		autoRecenterTimer_ = 0.0f;
	}

	// 角度も補間してからCamera位置へ反映し、周回を急な瞬間移動にしない
	orbitYaw_ = LerpAngle(orbitYaw_, targetOrbitYaw_, orbitT);
	orbitPitch_ += (targetOrbitPitch_ - orbitPitch_) * orbitT;
}

// 壁際では手動角度を残して上方へ逃がし、天井と外壁を避けた位置・視野角を反映します。
void CameraController::UpdateCameraTransform(
	float deltaTime,
	bool isTargetMoving,
	const std::vector<MyMath::OBB>& cameraCollisionObbs)
{

	// 手動角度を変更せず、横壁の自動回転と上方回避だけを一時的に足します。
	const float desiredYawOffset = isWallAvoidanceEnabled_ && isAutomaticWallViewEnabled_
		? CalculateWallAvoidanceYaw(cameraCollisionObbs) - orbitYaw_
		: 0.0f;
	const float yawT = 1.0f - std::exp(-kWallAvoidanceTurnSpeed * deltaTime);
	wallYawOffset_ += (desiredYawOffset - wallYawOffset_) * yawT;
	const float effectiveYaw = orbitYaw_ + wallYawOffset_;
	const float desiredPitchOffset = isWallAvoidanceEnabled_ && isAutomaticWallViewEnabled_
		? (std::max)(0.0f, CalculateWallAvoidancePitch(effectiveYaw, cameraCollisionObbs) - orbitPitch_)
		: 0.0f;
	const float pitchSpeed = desiredPitchOffset > wallPitchOffset_
		? kWallAvoidanceRiseSpeed : kWallAvoidanceReturnSpeed;
	const float pitchT = 1.0f - std::exp(-pitchSpeed * deltaTime);
	wallPitchOffset_ += (desiredPitchOffset - wallPitchOffset_) * pitchT;
	const float effectivePitch = std::clamp(
		orbitPitch_ + wallPitchOffset_, minimumOrbitPitch_, kWallAvoidanceMaximumPitch);
	wallPitchOffset_ = effectivePitch - orbitPitch_;

	// Camera の理想位置は Focus 基準で決めるため、Player が向くだけでは移動しない
	const Vector3 targetCameraPosition =
		CalculateCollisionSafeCameraPosition(cameraCollisionObbs, effectiveYaw, effectivePitch);
	const Vector3 idealCameraPosition = CalculateTargetCameraPosition(effectiveYaw, effectivePitch, distance_);
	const Vector3 avoidanceOffset{
		idealCameraPosition.x - targetCameraPosition.x,
		idealCameraPosition.y - targetCameraPosition.y,
		idealCameraPosition.z - targetCameraPosition.z,
	};
	const bool isAvoidingOuterWall = Length(avoidanceOffset) > 0.0001f;
	const float followSpeed = wallPitchOffset_ > 0.001f
		? kWallAvoidancePositionSpeed
		: (isAvoidingOuterWall ? wallAvoidanceFollowSpeed_ : cameraFollowSpeed_);
	const float cameraT = 1.0f - std::exp(-followSpeed * deltaTime);
	cameraPosition_ = Lerp(cameraPosition_, targetCameraPosition, cameraT);
	// 補間中に外壁を通り抜けるFrameを作らず、建物の外を映さないようにします。
	cameraPosition_ = ClampCameraPositionToCollisionBoundaries(
		cameraPosition_,
		cameraCollisionObbs);

	// 走っている間だけ少し広く映し、止まると通常の視野角へ戻す
	const float baseFovY = hasAreaSettings_ ? areaBaseFovY_ : baseFovY_;
	const float targetFovY = isTargetMoving ? baseFovY + (movingFovY_ - baseFovY_) : baseFovY;
	const float fovT = 1.0f - std::exp(-fovFollowSpeed_ * deltaTime);
	currentFovY_ += (targetFovY - currentFovY_) * fovT;
	camera_->SetFovY(currentFovY_);
	ApplyCameraTransform();
}

void CameraController::SetDistance(float distance)
{
	manualDistance_ = (std::max)(distance, 0.1f);
	distanceStepIndex_ = std::clamp(
		static_cast<int>(std::round((manualDistance_ - minimumManualDistance_) / manualDistanceStep_)),
		0,
		maximumDistanceStep_);
	if (!hasAreaSettings_) {
		targetDistance_ = manualDistance_;
	}
}

void CameraController::SetDistanceLock(bool locked)
{
	if (locked && !isDistanceLocked_) {
		// 鏡を持ち始めた瞬間に実際に見えている距離を保存します。
		// 目標値だけを保存すると、補間途中の近い目標へ離した瞬間に戻ってしまいます。
		lockedMinimumDistance_ = (std::max)(distance_, targetDistance_);
	}
	if (!locked && isDistanceLocked_) {
		// 鏡を離した後も、持っていた時に見やすかった距離を通常Cameraの基準へ残します。
		// 壁回避による一時的な接近だけはCalculateCollisionSafeCameraPositionが優先します。
		manualDistance_ = (std::max)(manualDistance_, lockedMinimumDistance_);
		targetDistance_ = (std::max)(targetDistance_, lockedMinimumDistance_);
	}
	isDistanceLocked_ = locked;
}

void CameraController::SetOrbitYaw(float orbitYaw)
{
	targetOrbitYaw_ = orbitYaw;
	orbitAnchorYaw_ = orbitYaw;
	orbitStepIndex_ = 0;
}

void CameraController::AddOrbitYaw(float deltaYaw)
{
	targetOrbitYaw_ += deltaYaw;
	orbitAnchorYaw_ = targetOrbitYaw_;
	orbitStepIndex_ = 0;
}

void CameraController::AddOrbitPitch(float deltaPitch)
{
	manualOrbitPitch_ = std::clamp(
		manualOrbitPitch_ + deltaPitch,
		minimumOrbitPitch_,
		maximumOrbitPitch_);
	if (!hasAreaSettings_) {
		targetOrbitPitch_ = manualOrbitPitch_;
	}
}

bool CameraController::TryStepOrbit(
	int stepDirection,
	const std::vector<MyMath::OBB>& cameraCollisionObbs)
{
	// 一回の方向転換角度は維持し、何回でも押せるのでCameraを一周以上回せます。
	const int nextStepIndex = orbitStepIndex_ + stepDirection;
	const float nextYaw = targetOrbitYaw_ + orbitStepAngle_ * static_cast<float>(stepDirection);
	if (!IsCameraSettingAvailable(
		nextYaw,
		targetOrbitPitch_,
		targetDistance_,
		cameraCollisionObbs)) {
		return false;
	}

	orbitStepIndex_ = nextStepIndex;
	targetOrbitYaw_ = nextYaw;
	return true;
}

bool CameraController::TryStepDistance(
	int stepDirection,
	const std::vector<MyMath::OBB>& cameraCollisionObbs)
{
	// 上キーは一段遠く、下キーは一段近くする
	const int nextStepIndex = std::clamp(
		distanceStepIndex_ + stepDirection,
		0,
		maximumDistanceStep_);
	if (nextStepIndex == distanceStepIndex_) {
		return false;
	}

	const float nextDistance =
		minimumManualDistance_ + manualDistanceStep_ * static_cast<float>(nextStepIndex);
	if (!IsCameraSettingAvailable(
		targetOrbitYaw_,
		targetOrbitPitch_,
		nextDistance,
		cameraCollisionObbs)) {
		return false;
	}

	distanceStepIndex_ = nextStepIndex;
	manualDistance_ = nextDistance;
	// Playerが矢印キーで近付けた時だけ、鏡を持った時の距離下限も変更を許可します。
	lockedMinimumDistance_ = (std::min)(lockedMinimumDistance_, manualDistance_);
	// CameraArea内でもPlayerの矢印キー操作を優先する
	targetDistance_ = manualDistance_;
	return true;
}

void CameraController::ResetBehindTarget(float targetFacingYaw)
{
	// 位置は補間するため、Rを押してもCameraは滑らかに後方へ戻る
	targetOrbitYaw_ = targetFacingYaw;
	orbitAnchorYaw_ = targetFacingYaw;
	orbitStepIndex_ = 0;
	autoRecenterTimer_ = 0.0f;
}

void CameraController::SynchronizeToCamera(const Vector3& targetPosition)
{
	if (!camera_) {
		return;
	}

	// 演出で表示していたCamera座標を保持し、通常追従へ切り替わる瞬間の移動をなくします。
	focus_ = {
		targetPosition.x + focusOffset_.x,
		targetPosition.y + focusOffset_.y,
		targetPosition.z + focusOffset_.z,
	};
	lookAt_ = focus_;
	cameraPosition_ = camera_->GetTranslate();

	const Vector3 cameraOffset{
		cameraPosition_.x - focus_.x,
		cameraPosition_.y - focus_.y,
		cameraPosition_.z - focus_.z,
	};
	const float distance = (std::max)(Length(cameraOffset), 0.1f);
	distance_ = distance;
	manualDistance_ = distance;
	targetDistance_ = distance;
	orbitPitch_ = std::asin(std::clamp(cameraOffset.y / distance, -1.0f, 1.0f));
	manualOrbitPitch_ = orbitPitch_;
	wallPitchOffset_ = 0.0f;
	wallYawOffset_ = 0.0f;
	targetOrbitPitch_ = orbitPitch_;
	orbitYaw_ = std::atan2(-cameraOffset.x, -cameraOffset.z);
	targetOrbitYaw_ = orbitYaw_;
	orbitAnchorYaw_ = orbitYaw_;
	orbitStepIndex_ = 0;
	autoRecenterTimer_ = 0.0f;
}

// 死亡前の手動角度やCamera Areaを残さず、Stage開始時と同じ追従設定を再構築します。
void CameraController::ResetToInitialState(const Vector3& targetPosition)
{
	Engine::Camera* camera = camera_;
	const bool autoRecenterEnabled = isAutoRecenterEnabled_;
	const bool wallAvoidanceEnabled = isWallAvoidanceEnabled_;
	*this = CameraController{};
	isAutoRecenterEnabled_ = autoRecenterEnabled;
	isWallAvoidanceEnabled_ = wallAvoidanceEnabled;
	Initialize(camera, targetPosition);
}

void CameraController::SetAreaSettings(const CameraAreaSettings& settings)
{
	// Areaの設定値へ即座に飛ばさず、Update内で現在値を補間して近づける
	hasAreaSettings_ = true;
	targetDistance_ = (std::max)(settings.distance, 0.1f);
	targetOrbitPitch_ = std::clamp(
		settings.pitch,
		minimumOrbitPitch_,
		maximumOrbitPitch_);
	areaBaseFovY_ = settings.fovY;
}

void CameraController::ClearAreaSettings()
{
	// Areaから出たら、その前にプレイヤーが選んでいた手動設定へ戻す
	hasAreaSettings_ = false;
	targetDistance_ = manualDistance_;
	targetOrbitPitch_ = manualOrbitPitch_;
	areaBaseFovY_ = baseFovY_;
}

Vector3 CameraController::CalculateTargetCameraPosition() const
{
	return CalculateTargetCameraPosition(orbitYaw_, orbitPitch_, distance_);
}

Vector3 CameraController::CalculateTargetCameraPosition(
	float yaw,
	float pitch,
	float distance) const
{
	const float horizontalDistance = distance * std::cos(pitch);
	return {
		focus_.x - std::sin(yaw) * horizontalDistance,
		focus_.y + std::sin(pitch) * distance,
		focus_.z - std::cos(yaw) * horizontalDistance,
	};
}

float CameraController::CalculateAvailablePathRatio(
	float yaw,
	float pitch,
	float distance,
	const std::vector<MyMath::OBB>& cameraCollisionObbs) const
{
	const Vector3 targetCameraPosition = CalculateTargetCameraPosition(yaw, pitch, distance);
	const Vector3 cameraPath{
		targetCameraPosition.x - focus_.x,
		targetCameraPosition.y - focus_.y,
		targetCameraPosition.z - focus_.z,
	};
	const float pathLength = Length(cameraPath);
	if (pathLength <= 0.0001f) {
		return 1.0f;
	}

	float nearestHitT = 1.0f;
	for (const MyMath::OBB& cameraCollisionObb : cameraCollisionObbs) {
		const Collision::SegmentHit hit = Collision::SegmentOBB(
			focus_,
			targetCameraPosition,
			cameraCollisionObb,
			cameraCollisionRadius_);
		if (hit.isHit) {
			nearestHitT = (std::min)(nearestHitT, hit.t);
		}
	}

	const float marginT = cameraCollisionMargin_ / pathLength;
	return std::clamp(nearestHitT - marginT, 0.0f, 1.0f);
}

bool CameraController::IsCameraSettingAvailable(
	float yaw,
	float pitch,
	float distance,
	const std::vector<MyMath::OBB>& cameraCollisionObbs) const
{
	const float candidateRatio = CalculateAvailablePathRatio(
		yaw,
		pitch,
		distance,
		cameraCollisionObbs);
	if (candidateRatio >= minimumAvailablePathRatio_) {
		return true;
	}

	// 壁際では、現在より壁から離れられる操作だけは受け付ける
	const float currentRatio = CalculateAvailablePathRatio(
		targetOrbitYaw_,
		targetOrbitPitch_,
		targetDistance_,
		cameraCollisionObbs);
	return candidateRatio > currentRatio + 0.05f;
}

// 通常視点が壁へ近付くほど上方候補を試し、天井も含めて見通せる距離を確保します。
float CameraController::CalculateWallAvoidancePitch(
	float yaw,
	const std::vector<MyMath::OBB>& cameraCollisionObbs) const
{
	if (cameraCollisionObbs.empty() || orbitPitch_ >= kWallAvoidanceMaximumPitch) {
		return orbitPitch_;
	}

	const float normalSafeDistance = distance_ * CalculateAvailablePathRatio(
		yaw, orbitPitch_, distance_, cameraCollisionObbs);
	if (normalSafeDistance >= kWallAvoidanceStartDistance) {
		return orbitPitch_;
	}

	float bestPitch = orbitPitch_;
	float bestDistance = normalSafeDistance;
	for (int sample = 1; sample <= kWallAvoidancePitchSamples; ++sample) {
		const float pitch = orbitPitch_ +
			(kWallAvoidanceMaximumPitch - orbitPitch_) *
			static_cast<float>(sample) / static_cast<float>(kWallAvoidancePitchSamples);
		const float safeDistance = distance_ * CalculateAvailablePathRatio(
			yaw, pitch, distance_, cameraCollisionObbs);
		if (safeDistance >= kWallAvoidanceStartDistance) {
			return pitch;
		}
		if (safeDistance > bestDistance) {
			bestDistance = safeDistance;
			bestPitch = pitch;
		}
	}
	return bestPitch;
}

// 横のCameraBoundaryに遮られた時だけ、壁を越えず距離を取り戻す向きへゆっくり回します。
float CameraController::CalculateWallAvoidanceYaw(
	const std::vector<MyMath::OBB>& cameraCollisionObbs) const
{
	if (cameraCollisionObbs.empty()) {
		return orbitYaw_;
	}

	const Vector3 normalTarget = CalculateTargetCameraPosition(orbitYaw_, orbitPitch_, distance_);
	const MyMath::OBB* nearestBoundary = nullptr;
	float nearestHitT = 1.0f;
	for (const MyMath::OBB& boundary : cameraCollisionObbs) {
		const Collision::SegmentHit hit = Collision::SegmentOBB(
			focus_, normalTarget, boundary, cameraCollisionRadius_);
		if (hit.isHit && hit.t < nearestHitT) {
			nearestHitT = hit.t;
			nearestBoundary = &boundary;
		}
	}
	if (!nearestBoundary) {
		return orbitYaw_;
	}

	// 最も薄い軸がX方向の外壁だけを横壁とし、天井や前後の壁では自動回転しません。
	int normalAxis = 0;
	if (nearestBoundary->size.y < nearestBoundary->size.x) {
		normalAxis = 1;
	}
	if (nearestBoundary->size.z <
		(normalAxis == 0 ? nearestBoundary->size.x : nearestBoundary->size.y)) {
		normalAxis = 2;
	}
	const Vector3 wallNormal = nearestBoundary->orientations[normalAxis];
	if (std::abs(wallNormal.x) <= (std::max)(std::abs(wallNormal.y), std::abs(wallNormal.z))) {
		return orbitYaw_;
	}

	const float normalSafeDistance = distance_ * CalculateAvailablePathRatio(
		orbitYaw_, orbitPitch_, distance_, cameraCollisionObbs);
	if (normalSafeDistance >= kWallAvoidanceStartDistance) {
		return orbitYaw_;
	}

	float bestYaw = orbitYaw_;
	float bestDistance = normalSafeDistance;
	// 左右が同じだけ空いている場合は、まず現在の手動角度が0へ近付く側を試します。
	const float preferredSign = std::abs(wallYawOffset_) > 0.01f
		? (wallYawOffset_ > 0.0f ? 1.0f : -1.0f)
		: (std::remainder(orbitYaw_, 2.0f * std::numbers::pi_v<float>) > 0.0f ? -1.0f : 1.0f);
	for (int sample = 1; sample <= kWallAvoidanceYawSamples; ++sample) {
		for (const float sign : { preferredSign, -preferredSign }) {
			const float yaw = orbitYaw_ + sign * kWallAvoidanceYawStep * static_cast<float>(sample);
			const float safeDistance = distance_ * CalculateAvailablePathRatio(
				yaw, orbitPitch_, distance_, cameraCollisionObbs);
			if (safeDistance >= kWallAvoidanceStartDistance) {
				return yaw;
			}
			if (safeDistance > bestDistance + 0.05f) {
				bestDistance = safeDistance;
				bestYaw = yaw;
			}
		}
	}
	return bestYaw;
}

// 選んだ上方角度でも外壁・天井を越えず、最初の接触位置の少し手前で止めます。
Vector3 CameraController::CalculateCollisionSafeCameraPosition(
	const std::vector<MyMath::OBB>& cameraCollisionObbs,
	float yaw,
	float pitch) const
{
	const Vector3 targetCameraPosition = CalculateTargetCameraPosition(yaw, pitch, distance_);
	// 壁越しシルエットを使うSceneでは、CameraをPlayerへ近付けず通常距離を維持します。
	if (!isWallAvoidanceEnabled_) {
		return targetCameraPosition;
	}
	const Vector3 cameraPath{
		targetCameraPosition.x - focus_.x,
		targetCameraPosition.y - focus_.y,
		targetCameraPosition.z - focus_.z,
	};
	const float pathLength = Length(cameraPath);
	if (pathLength <= 0.0001f) {
		return targetCameraPosition;
	}

	// CameraとFocusの間に壁がある場合は、壁の少し手前まで近づける
	const float safeT = CalculateAvailablePathRatio(
		yaw,
		pitch,
		distance_,
		cameraCollisionObbs);
	return {
		focus_.x + cameraPath.x * safeT,
		focus_.y + cameraPath.y * safeT,
		focus_.z + cameraPath.z * safeT,
	};
}

// 現在位置が外壁の外側へ補間されそうな時だけ、最初の外壁の手前へ即座に収めます。
Vector3 CameraController::ClampCameraPositionToCollisionBoundaries(
	const Vector3& cameraPosition,
	const std::vector<MyMath::OBB>& cameraCollisionObbs) const
{
	if (!isWallAvoidanceEnabled_ || cameraCollisionObbs.empty()) {
		return cameraPosition;
	}

	const Vector3 cameraPath{
		cameraPosition.x - focus_.x,
		cameraPosition.y - focus_.y,
		cameraPosition.z - focus_.z,
	};
	const float pathLength = Length(cameraPath);
	if (pathLength <= 0.0001f) {
		return cameraPosition;
	}

	float nearestHitT = 1.0f;
	for (const MyMath::OBB& cameraCollisionObb : cameraCollisionObbs) {
		const Collision::SegmentHit hit = Collision::SegmentOBB(
			focus_,
			cameraPosition,
			cameraCollisionObb,
			cameraCollisionRadius_);
		if (hit.isHit) {
			nearestHitT = (std::min)(nearestHitT, hit.t);
		}
	}

	const float safeT = std::clamp(
		nearestHitT - cameraCollisionMargin_ / pathLength,
		0.0f,
		1.0f);
	return {
		focus_.x + cameraPath.x * safeT,
		focus_.y + cameraPath.y * safeT,
		focus_.z + cameraPath.z * safeT,
	};
}

void CameraController::ApplyCameraTransform()
{
	if (!camera_) {
		return;
	}

	// Camera の位置から視線専用の注視点へのベクトルを、X と Y の回転角へ変換する
	const Vector3 lookDirection = Normalize({
		lookAt_.x - cameraPosition_.x,
		lookAt_.y - cameraPosition_.y,
		lookAt_.z - cameraPosition_.z,
	});
	const float pitch = -std::asin(std::clamp(lookDirection.y, -1.0f, 1.0f));
	const float yaw = std::atan2(lookDirection.x, lookDirection.z);

	camera_->SetTranslate(cameraPosition_);
	camera_->SetRotate({ pitch, yaw, 0.0f });
}

float CameraController::LerpAngle(float current, float target, float t) const
{
	return current + std::remainder(target - current, 2.0f * std::numbers::pi_v<float>) * t;
}

float CameraController::MoveTowardsAngle(float current, float target, float maxDelta) const
{
	const float difference = std::remainder(target - current, 2.0f * std::numbers::pi_v<float>);
	if (std::abs(difference) <= maxDelta) {
		return current + difference;
	}
	return current + (difference > 0.0f ? maxDelta : -maxDelta);
}
}
