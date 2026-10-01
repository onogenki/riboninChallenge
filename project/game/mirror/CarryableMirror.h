#pragma once

#include "Collider.h"
#include "Mirror.h"
#include "MyMath.h"
#include "Object3d.h"
#include <string>

class Object3dCommon;

// Playerが拾って運べる、小型のレーザー反射用Mirrorです。
class CarryableMirror
{
public:
	// 持った時の位置だけをまとめます。ここに書いた初期値がゲーム開始時の配置です。
	struct HoldSettings
	{
		float verticalDistance = 2.5f;
		float horizontalDistance = 4.0f;
		float verticalSide = 0.0f;
		float verticalHeight = 0.5f;
		float horizontalHeight = 0.35f;
	};
	// Editorの調整値を次のUpdateから使います。鏡の大きさや反射判定は変えません。
	void SetHoldSettings(const HoldSettings& settings) { holdSettings_ = settings; }
	// Editorが現在の持ち位置を読み、画面へ表示します。
	const HoldSettings& GetHoldSettings() const { return holdSettings_; }

	// ModelとColliderの初期化に成功した時だけtrueを返します。
	// falseならStageは鏡なしで続行せず、原因を初期化時に止められます。
	bool Initialize(
		Object3dCommon* object3dCommon,
		const std::string& modelName,
		const Vector3& startPosition,
		float width,
		float height);
	// interactPressedはEキーなどの「押した瞬間」だけtrueを渡します。
	void Update(
		float deltaTime,
		const Vector3& playerPosition,
		float playerFacingYaw,
		bool interactPressed,
		bool isAiming = false,
		float horizontalAimInput = 0.0f,
		bool isRightMouseHeld = false,
		float verticalAimInput = 0.0f);

	Object3d& GetObject() { return object_; }
	const Object3d& GetObject() const { return object_; }
	const Mirror& GetMirror() const { return mirror_; }
	// Playerなどが持てる鏡のColliderへ判定を依頼する時に使います。
	const ObbCollider& GetCollider() const { return collider_; }
	// 床・壁一覧やLaser遮蔽へ渡す、持てる鏡のOBB形状データを返します。
	const MyMath::OBB& GetObb() const { return collider_.GetShape(); }
	bool IsCarried() const { return isCarried_; }
	// Stage開始時の装備状態やEventから、鏡を持つ・置く状態へ明示的に切り替えます。
	void SetCarried(bool isCarried);
	// stage1.jsonで指定した位置へ、置かれているMirrorだけを移動します。
	void SetDroppedPosition(const Vector3& position);

private:
	void ApplyTransform(const Vector3& position, float pitch, float yaw);

	Object3d object_;
	Mirror mirror_;
	// 持つ・置くのどちらでも、Object3dのTransformに追従するOBB Colliderです。
	ObbCollider collider_{};
	float width_ = 1.4f;
	float height_ = 1.0f;
	float yaw_ = 0.0f;
	// 横持ちから置いた直後の鏡も、同じ位置から拾い直せる距離です。
	float pickupDistance_ = 4.0f;
	HoldSettings holdSettings_{};
	// Playerが旋回した時に、鏡が目標方向へ追従する速さです。
	float holdTurnFollowSpeed_ = 9.0f;
	// 左クリック中にMouseを動かした時、MirrorをPlayerの前方で横へ回す速さです。
	float aimSensitivity_ = 0.012f;
	// MirrorはPlayerの真横までは回らないよう、前方を中心とした範囲に制限します。
	float maxAimYawOffset_ = 1.00f;
	// Playerの正面から見たMirrorの左右角度です。左クリックを離すと正面へ戻ります。
	float aimYawOffset_ = 0.0f;
	// 右クリックで寝かせて持つ状態です。左クリック中は通常のガード状態へ戻ります。
	bool isHorizontalHoldMode_ = false;
	// trueなら鏡面の表側を上へ、falseなら下へ向けます。
	// 最初の右クリックでは上向きになるよう、初期値は下向きにします。
	bool isHorizontalFacingUp_ = false;
	// 右クリック中のMouse上下移動で加える、水平Mirrorの傾きです。
	float horizontalTilt_ = 0.0f;
	float horizontalTiltMouseSensitivity_ = 0.006f;
	float maxHorizontalTilt_ = 0.85f;
	// 右クリックを短く離した時だけ、上向き・下向きの切替として扱います。
	float rightMouseHoldTime_ = 0.0f;
	float rightMouseClickThreshold_ = 0.22f;
	bool wasRightMouseHeld_ = false;
	bool isCarried_ = false;
};
