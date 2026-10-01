#include "Player.h"

#include "Collision.h"
#include "Input.h"
#include "MyMath.h"
#include "Object3dFactory.h"
#include <algorithm>
#include <cmath>
#include <dinput.h>

using namespace MyMath;

// Playerのモデルと球Colliderを同じ開始位置へ作り、成功したかを呼び出し元へ返します。
bool Player::Initialize(
	Object3dCommon* object3dCommon,
	const std::string& modelName,
	const Vector3& startPosition,
	float radius)
{
	// 値として所有する球モデルも、Factoryの共通初期化経路を使います。
	if (!Object3dFactory::InitializeObject(object_, object3dCommon, modelName)) {
		return false;
	}

	//開始位置と大きさをColliderへ設定する
	position_ = startPosition;
	collider_.SetShape(position_, radius);

	//sphere.objは半径0.5のモデルなので、当たり判定の半径と見た目の半径を揃える
	constexpr float kSphereModelRadius = 0.5f;
	const float visualScale = collider_.GetShape().radius / kSphereModelRadius;
	object_.SetScale({ visualScale, visualScale, visualScale });
	object_.SetTranslate(position_);
	return true;
}

void Player::Update(
	float deltaTime,
	const std::vector<OBB>& solidObbs,
	const Vector3& cameraForward)
{
	// Keyboard入力を共通のControlInputへ変換してから、移動・重力・衝突処理へ渡します。
	UpdateWithControl(deltaTime, solidObbs, cameraForward, ReadControlInput());
}

// Keyboard入力を、機器に依存しないControlInputへ変換します。
Player::ControlInput Player::ReadControlInput()
{
	ControlInput controlInput{};
	Input* input = Input::GetInstance();
	if (input->PushKey(DIK_W)) {
		controlInput.forward += 1.0f;
	}
	if (input->PushKey(DIK_S)) {
		controlInput.forward -= 1.0f;
	}
	if (input->PushKey(DIK_A)) {
		controlInput.right -= 1.0f;
	}
	if (input->PushKey(DIK_D)) {
		controlInput.right += 1.0f;
	}
	controlInput.jumpPressed = input->TriggerKey(DIK_SPACE);
	return controlInput;
}

// 演出やリスポーンでPlayerを移動し、速度・Collider・見た目を同じ位置へそろえます。
void Player::SetPosition(const Vector3& position)
{
	// 演出中に直前の落下速度や入力方向を持ち越さないよう、移動状態も初期化します。
	position_ = position;
	velocity_ = {};
	moveDirection_ = {};
	isGrounded_ = false;
	isColliding_ = false;
	collider_.SetCenter(position_);
	object_.SetTranslate(position_);
}

// 入力値を受け取り、移動・重力・衝突解決を順番に実行します。
void Player::UpdateWithControl(
	float deltaTime,
	const std::vector<OBB>& solidObbs,
	const Vector3& cameraForward,
	const ControlInput& controlInput)
{
	const float forwardInput = std::clamp(controlInput.forward, -1.0f, 1.0f);
	const float rightInput = std::clamp(controlInput.right, -1.0f, 1.0f);

	// Cameraが上下を向いていても、移動には床と平行なX-Z方向だけを使う
	Vector3 forwardOnGround{ cameraForward.x, 0.0f, cameraForward.z };
	if (Length(forwardOnGround) <= 0.0001f) {
		forwardOnGround = { 0.0f, 0.0f, 1.0f };
	}
	forwardOnGround = Normalize(forwardOnGround);
	const Vector3 rightOnGround{ forwardOnGround.z, 0.0f, -forwardOnGround.x };

	// Wなら画面奥、Dなら画面右へ向く世界座標の移動方向を作る
	moveDirection_ = {
		forwardOnGround.x * forwardInput + rightOnGround.x * rightInput,
		0.0f,
		forwardOnGround.z * forwardInput + rightOnGround.z * rightInput,
	};

	// 斜め移動だけ速くならないよう、方向だけを正規化する
	if (Length(moveDirection_) > 0.0f) {
		moveDirection_ = Normalize(moveDirection_);
		// 構え中は盾を構えたまま歩くように移動を遅くします。
		float currentMoveSpeed = movementSettings_.moveSpeed;
		if (isMirrorGuardMode_) {
			currentMoveSpeed *= movementSettings_.mirrorGuardMoveSpeedRate;
		} else {
			// 既に向いている前方へ進む時だけ、少しだけ歩行を速くします。
			const Vector3 facingForward{ std::sin(facingYaw_), 0.0f, std::cos(facingYaw_) };
			if (Dot(facingForward, moveDirection_) > 0.85f) {
				currentMoveSpeed *= 1.12f;
			}
		}
		position_.x += moveDirection_.x * currentMoveSpeed * deltaTime;
		position_.z += moveDirection_.z * currentMoveSpeed * deltaTime;

		// 構えていない時だけ、球の見た目を移動方向へ向けます。
		if (!isMirrorGuardMode_) {
			facingYaw_ = std::atan2(moveDirection_.x, moveDirection_.z);
			object_.SetRotate({ 0.0f, facingYaw_, 0.0f });
		}
	}

	//床に立っているときだけ、Spaceで上向きの速さを与える
	if (isGrounded_ && controlInput.jumpPressed) {
		velocity_.y = movementSettings_.jumpSpeed *
			(isMirrorGuardMode_ ? movementSettings_.mirrorGuardJumpSpeedRate : 1.0f);
		isGrounded_ = false;
	}

	//重力で毎フレームのY方向の速さを変化させ、位置へ反映する
	velocity_.y += movementSettings_.gravity * deltaTime;
	position_.y += velocity_.y * deltaTime;

	// 床・壁・鏡との球対箱の押し戻しを、Playerが所有するColliderへ一度だけ依頼します。
	collider_.SetCenter(position_);
	const Collision::SphereObbResolution collisionResult =
		collider_.ResolveSolidObbs(velocity_, solidObbs);
	position_ = collider_.GetShape().center;
	isGrounded_ = collisionResult.isGrounded;
	isColliding_ = collisionResult.isCollision;

	// 衝突解決済みの位置を、画面に描く球モデルへ反映します。
	// Colliderの中心はResolveSolidObbsが更新済みなので、ここで再設定しません。
	object_.SetTranslate(position_);
}
