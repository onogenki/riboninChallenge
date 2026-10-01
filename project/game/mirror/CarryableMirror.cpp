#include "CarryableMirror.h"

#include "Collision.h"
#include "MyMath.h"
#include "Object3dCommon.h"
#include "Object3dFactory.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

bool CarryableMirror::Initialize(
	Object3dCommon* object3dCommon,
	const std::string& modelName,
	const Vector3& startPosition,
	float width,
	float height)
{
	width_ = (std::max)(width, 0.1f);
	height_ = (std::max)(height, 0.1f);
	// 値として所有するMirrorモデルも、Factoryの共通初期化経路を使います。
	if (!Object3dFactory::InitializeObject(object_, object3dCommon, modelName)) {
		return false;
	}
	// 持てるMirrorは景色を映さないLaser用の鏡です。UVチェッカーではなく、反射前の鏡らしい明るい板を表示します。
	object_.SetTextureOverride("resources/debug/white.png");
	// Laserを反射しない裏側だけを少し暗くし、持ち方にかかわらず表裏を区別します。
	object_.SetBackFaceBrightness(0.35f);
	// 小型Mirrorも表側だけLaserを反射します。
	mirror_.SetReflectBackface(false);
	// 薄い鏡板と同じ形をColliderにも使い、見た目と当たり判定を一致させます。
	collider_.SetLocalShape({}, { 1.0f, 1.0f, 0.05f });
	ApplyTransform(startPosition, 0.0f, 3.14159265f);
	return true;
}

// 入力で持ち方を切り替え、HoldSettingsの位置へ見た目・Laser面・Colliderを一緒に動かします。
void CarryableMirror::Update(
	float deltaTime,
	const Vector3& playerPosition,
	float playerFacingYaw,
	bool interactPressed,
	bool isAiming,
	float horizontalAimInput,
	bool isRightMouseHeld,
	float verticalAimInput)
{
	if (interactPressed) {
		if (isCarried_) {
			isCarried_ = false;
			wasRightMouseHeld_ = false;
			rightMouseHoldTime_ = 0.0f;
		} else {
			const Vector3 difference{
				mirror_.GetCenter().x - playerPosition.x,
				mirror_.GetCenter().y - playerPosition.y,
				mirror_.GetCenter().z - playerPosition.z,
			};
			if (Length(difference) <= pickupDistance_) {
				isCarried_ = true;
				aimYawOffset_ = 0.0f;
				isHorizontalHoldMode_ = false;
				horizontalTilt_ = 0.0f;
			}
		}
	}

	if (isCarried_) {
		// 左クリックは常にPlayer全体を守る、縦向きの通常Mirrorへ戻します。
		if (isAiming) {
			isHorizontalHoldMode_ = false;
		}
		if (isRightMouseHeld) {
			rightMouseHoldTime_ += (std::max)(deltaTime, 0.0f);
			if (isHorizontalHoldMode_) {
				// Mouseを下へ動かすと、下から来たLightをPlayer前方へ返す向きへ傾きます。
				horizontalTilt_ -= verticalAimInput * horizontalTiltMouseSensitivity_;
				horizontalTilt_ = std::clamp(horizontalTilt_, -maxHorizontalTilt_, maxHorizontalTilt_);
			}
		} else if (wasRightMouseHeld_ && rightMouseHoldTime_ <= rightMouseClickThreshold_) {
			// 短い右クリックを離した時だけ、上向き・下向きの水平Mirrorを切り替えます。
			isHorizontalHoldMode_ = true;
			isHorizontalFacingUp_ = !isHorizontalFacingUp_;
			horizontalTilt_ = 0.0f;
		}
		if (!isRightMouseHeld) {
			rightMouseHoldTime_ = 0.0f;
		}
		wasRightMouseHeld_ = isRightMouseHeld;

		const bool isRotatingHeldMirror = isAiming || isRightMouseHeld;
		if (isRotatingHeldMirror) {
			// Mouseを右へ動かすと、縦向き・水平MirrorともPlayerの右前方へ動きます。
			aimYawOffset_ += horizontalAimInput * aimSensitivity_;
			aimYawOffset_ = std::clamp(aimYawOffset_, -maxAimYawOffset_, maxAimYawOffset_);
		} else {
			// 構える操作を終えると、MirrorはPlayerの正面へ滑らかに戻ります。
			const float returnRate = 1.0f - std::exp(-holdTurnFollowSpeed_ * (std::max)(deltaTime, 0.0f));
			aimYawOffset_ += (0.0f - aimYawOffset_) * returnRate;
		}

		const float targetYaw = playerFacingYaw + aimYawOffset_;
		// 位置だけ先に旋回すると板がPlayer正面から横へずれるため、位置と向きを同じ角度で補間します。
		const float yawDifference = std::remainder(targetYaw - yaw_, 2.0f * 3.14159265f);
		const float followSpeed = isRotatingHeldMirror ? holdTurnFollowSpeed_ * 2.5f : holdTurnFollowSpeed_;
		const float followRate = 1.0f - std::exp(-followSpeed * (std::max)(deltaTime, 0.0f));
		const float heldYaw = yaw_ + yawDifference * followRate;
		const Vector3 playerForward{
			std::sin(heldYaw),
			0.0f,
			std::cos(heldYaw),
		};
		// 左右の調整値が0なら、縦向きMirrorもPlayerの正面に置きます。
		const Vector3 playerRight{
			playerForward.z,
			0.0f,
			-playerForward.x,
		};
		const float heldDistance = isHorizontalHoldMode_
			? holdSettings_.horizontalDistance : holdSettings_.verticalDistance;
		const float heldHeight = isHorizontalHoldMode_
			? holdSettings_.horizontalHeight : holdSettings_.verticalHeight;
		const float heldSideOffset = isHorizontalHoldMode_ ? 0.0f : holdSettings_.verticalSide;
		const Vector3 heldPosition{
			playerPosition.x + playerForward.x * heldDistance + playerRight.x * heldSideOffset,
			playerPosition.y + heldHeight,
			playerPosition.z + playerForward.z * heldDistance + playerRight.z * heldSideOffset,
		};
		// 同じheldYawをLaser面・見た目・Colliderへ渡し、持ち替え中も三者をそろえます。
		const float horizontalBasePitch = isHorizontalFacingUp_ ? -1.57079633f : 1.57079633f;
		const float targetPitch = isHorizontalHoldMode_ ? horizontalBasePitch + horizontalTilt_ : 0.0f;
		ApplyTransform(heldPosition, targetPitch, heldYaw);
	} else {
		// 落とした後もColliderを現在のTransformへ追従させます。
		wasRightMouseHeld_ = false;
		rightMouseHoldTime_ = 0.0f;
		collider_.SyncTransform(object_.GetTransform());
	}
}

void CarryableMirror::SetDroppedPosition(const Vector3& position)
{
	if (isCarried_) {
		// 持っている最中のJSON再読込で、手元のMirrorを地面へ戻さないようにします。
		return;
	}

	ApplyTransform(position, 0.0f, 3.14159265f);
}

void CarryableMirror::SetCarried(bool isCarried)
{
	isCarried_ = isCarried;
	// 持ち直す時は、前回の構え・傾きを残さず通常の縦向きMirrorから始めます。
	if (isCarried_) {
		aimYawOffset_ = 0.0f;
		isHorizontalHoldMode_ = false;
		horizontalTilt_ = 0.0f;
	}
}

void CarryableMirror::ApplyTransform(const Vector3& position, float pitch, float yaw)
{
	// 引数で受け取った向きを保持し、見た目・Laser用Mirror・Colliderを同じ向きへそろえます。
	yaw_ = yaw;
	object_.SetTranslate(position);
	object_.SetRotate({ pitch, yaw_, 0.0f });
	// plane.objは幅・高さが2.0なので、半分のScaleで指定サイズへそろえます。
	object_.SetScale({ width_ * 0.5f, height_ * 0.5f, 1.0f });

	mirror_.SetCenter(position);
	mirror_.SetSize(width_, height_);
	const Matrix4x4 rotationMatrix = MakeAffineMatrix(
		{ 1.0f, 1.0f, 1.0f },
		{ pitch, yaw_, 0.0f },
		{ 0.0f, 0.0f, 0.0f });
	mirror_.SetNormal({
		rotationMatrix.m[2][0],
		rotationMatrix.m[2][1],
		rotationMatrix.m[2][2],
	});
	collider_.SyncTransform(object_.GetTransform());
}
