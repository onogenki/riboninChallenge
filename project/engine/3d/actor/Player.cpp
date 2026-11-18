#include "Player.h"

#include "Collision.h"
#include "Input.h"
#include "MyMath.h"
#include "ModelManager.h"
#include "Object3dFactory.h"
#include <algorithm>
#include <cmath>
#include <dinput.h>
#include <numbers>

using namespace MyMath;

namespace
{
	// OBJで作った床からの浮きは維持し、揺れは見た目だけに小さく足します。
	constexpr float kIdleFloatHeight = 0.16f;
	constexpr float kMovingFloatHeight = 0.22f;
	constexpr float kIdleFloatPeriod = 2.1f;
	constexpr float kMovingFloatPeriod = 1.45f;
	constexpr float kFloatBlendSpeed = 8.0f;
}

// PlayerのOBJを元の大きさで表示し、体を囲む球Colliderをモデル原点からずらして作ります。
bool Player::Initialize(
	Engine::Object3dCommon* object3dCommon,
	const std::string& modelName,
	const Vector3& startPosition,
	float radius,
	const Vector3& colliderCenterOffset,
	const std::string& leftModelName)
{
	// 左右のどちらかが欠けた状態で切替を始めず、右モデルを先に表示します。
	if (!leftModelName.empty() &&
		!Engine::ModelManager::GetInstance()->LoadModel(leftModelName)) {
		return false;
	}
	// 値として所有するPlayerモデルも、Factoryの共通初期化経路を使います。
	if (!Engine::Object3dFactory::InitializeObject(object_, object3dCommon, modelName)) {
		return false;
	}
	rightModelName_ = modelName;
	leftModelName_ = leftModelName;
	isLeftModel_ = false;

	// モデル原点とCollider中心を分け、Blenderで作った浮遊量を床判定で消さないようにします。
	position_ = startPosition;
	colliderCenterOffset_ = colliderCenterOffset;
	collider_.SetShape(Add(position_, colliderCenterOffset_), radius);

	// Blenderから書き出したPlayerの頂点寸法を、接地用Colliderの半径で拡大せずそのまま表示します。
	object_.SetScale({ 1.0f, 1.0f, 1.0f });
	// 普段は輪郭だけ淡く光らせ、死亡・Goalの強い発光とは強さを分けます。
	SetVisualSpecularGlow(1.0f, 0.0f);
	// 体の白い縁は0.55、ピンクのリボンの縁はその25%に抑えます。
	object_.SetGhostRim(0.55f, 0.25f);
	floatPhase_ = 0.0f;
	floatAmplitude_ = 0.0f;
	floatOffsetY_ = 0.0f;
	SyncVisualPosition();
	return true;
}

// 調整値を一括して所有し、更新中に外部から内部設定を直接書き換えられないようにします。
void Player::SetMovementSettings(const MovementSettings& settings)
{
	movementSettings_ = settings;
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
	Engine::Input* input = Engine::Input::GetInstance();
	if (input->PushKey(DIK_W)) {
		controlInput.forward += 1.0f;
	}
	if (input->PushKey(DIK_S)) {
		controlInput.forward -= 1.0f;
	}
	controlInput.leftPressed = input->PushKey(DIK_A);
	controlInput.rightPressed = input->PushKey(DIK_D);
	if (controlInput.leftPressed) {
		controlInput.right -= 1.0f;
	}
	if (controlInput.rightPressed) {
		controlInput.right += 1.0f;
	}
	controlInput.jumpPressed = input->TriggerKey(DIK_SPACE);
	return controlInput;
}

// 入力方向だけで見た目のOBJを選び、移動方向・姿勢・Colliderは変えません。
void Player::UpdateFacingModel(const ControlInput& controlInput)
{
	if (leftModelName_.empty()) {
		return;
	}
	const bool hasDirectionKey = controlInput.leftPressed || controlInput.rightPressed;
	if (!hasDirectionKey && controlInput.right == 0.0f) {
		// A・Dを離した後も、最後に選んだ向きの見た目を保ちます。
		return;
	}
	const bool shouldUseLeft = hasDirectionKey
		? controlInput.leftPressed && !controlInput.rightPressed
		: controlInput.right < 0.0f;
	if (shouldUseLeft != isLeftModel_) {
		if (object_.SetLoadedModel(shouldUseLeft ? leftModelName_ : rightModelName_)) {
			isLeftModel_ = shouldUseLeft;
		}
	}
}

// 外部の演出がPlayerの位置やColliderに触れず、透明度だけを変えられるようにします。
void Player::SetVisualOpacity(float opacity)
{
	object_.SetOpacity(opacity);
}

// Player本体の通常反射を少し抑えつつ、死亡・Goal演出の反射と発光も同じ個体へ渡します。
void Player::SetVisualSpecularGlow(float specularBoost, float glowIntensity)
{
	object_.SetSpecularGlow(specularBoost * 0.70f, glowIntensity);
}

// 危険Lightの方向と強さを、Playerが所有するモデルへ渡します。
void Player::SetVisualLightWarning(const Vector3& worldDirection, float strength)
{
	object_.SetLightWarning(worldDirection, strength);
}

// 演出やリスポーンでPlayerを移動し、Collider中心のずれを保ちながら見た目と速度をそろえます。
void Player::SetPosition(const Vector3& position)
{
	// 演出中に直前の落下速度や入力方向を持ち越さないよう、移動状態も初期化します。
	position_ = position;
	velocity_ = {};
	moveDirection_ = {};
	isGrounded_ = false;
	isColliding_ = false;
	collider_.SetCenter(Add(position_, colliderCenterOffset_));
	SyncVisualPosition();
}

// ワープ後に直前の落下速度や構え状態が残らないよう、Playerの初期状態を一度に復元します。
void Player::ResetForRespawn(const Vector3& position)
{
	SetPosition(position);
	facingYaw_ = 0.0f;
	// 死亡前に左を向いていても、開始時と同じ右モデルへ戻します。
	if (isLeftModel_) {
		if (object_.SetLoadedModel(rightModelName_)) {
			isLeftModel_ = false;
		}
	}
	isMirrorGuardMode_ = false;
	object_.SetRotate({ 0.0f, 0.0f, 0.0f });
	object_.SetOpacity(1.0f);
	// 復帰後も通常のお化けの輪郭光へ戻します。
	SetVisualSpecularGlow(1.0f, 0.0f);
	object_.SetLightWarning({ 0.0f, 0.0f, 1.0f }, 0.0f);
	// 復帰直後に死亡前の上下量を残さず、床から自然に揺れ始めます。
	floatPhase_ = 0.0f;
	floatAmplitude_ = 0.0f;
	floatOffsetY_ = 0.0f;
	SyncVisualPosition();
}

// 上向きのジャンプ中は触れず、Colliderの下端だけをGoalの曲面へ接地させます。
void Player::ApplyWalkableSurface(float surfaceY)
{
	const float centerY = surfaceY + collider_.GetShape().radius - colliderCenterOffset_.y;
	if (velocity_.y > 0.0f || position_.y > centerY + 0.12f) {
		return;
	}
	position_.y = centerY;
	velocity_.y = 0.0f;
	isGrounded_ = true;
	isColliding_ = true;
	collider_.SetCenter(Add(position_, colliderCenterOffset_));
	SyncVisualPosition();
}

// 入力値を受け取り、移動・重力・衝突解決を順番に実行します。
void Player::UpdateWithControl(
	float deltaTime,
	const std::vector<OBB>& solidObbs,
	const Vector3& cameraForward,
	const ControlInput& controlInput)
{
	// 斜め移動や入力相殺より先に見た目を選び、A+Dでは右を優先します。
	UpdateFacingModel(controlInput);
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

		// 構えていない時だけ進行方向へ向け、鏡の水平持ちで付けた体の傾きは保ちます。
		if (!isMirrorGuardMode_) {
			facingYaw_ = std::atan2(moveDirection_.x, moveDirection_.z);
			object_.SetRotate({ object_.GetRotate().x, facingYaw_, 0.0f });
		}
	}

	//床に立っているときだけ、Spaceで上向きの速さを与える
	const bool jumpedThisFrame = isGrounded_ && controlInput.jumpPressed;
	if (jumpedThisFrame) {
		velocity_.y = movementSettings_.jumpSpeed *
			(isMirrorGuardMode_ ? movementSettings_.mirrorGuardJumpSpeedRate : 1.0f);
		isGrounded_ = false;
	}

	//重力で毎フレームのY方向の速さを変化させ、位置へ反映する
	velocity_.y += movementSettings_.gravity * deltaTime;
	position_.y += velocity_.y * deltaTime;

	// 床・壁・鏡との押し戻しは体のColliderへ行い、モデル原点の浮遊量を保ちます。
	collider_.SetCenter(Add(position_, colliderCenterOffset_));
	const Collision::SphereObbResolution collisionResult =
		collider_.ResolveSolidObbs(velocity_, solidObbs);
	position_ = Subtract(collider_.GetShape().center, colliderCenterOffset_);
	isGrounded_ = collisionResult.isGrounded;
	isColliding_ = collisionResult.isCollision;

	// ジャンプ中は揺れ幅を消し、着地後に戻してジャンプ本来の上下動を見せます。
	UpdateVisualFloat(deltaTime, isGrounded_ && !jumpedThisFrame);
	// Colliderの中心はResolveSolidObbsが更新済みなので、ここで再設定しません。
	SyncVisualPosition();
}

// 鏡の回転に体全体を小さく追従させ、盾・水平持ちの操作を姿勢で伝えます。
void Player::UpdateMirrorPose(float deltaTime, bool isCarried, float mirrorYaw, float mirrorPitch)
{
	const float frameTime = std::clamp(deltaTime, 0.0f, 0.1f);
	if (frameTime <= 0.0f) {
		return;
	}
	const Vector3 currentRotation = object_.GetRotate();
	const float mirrorYawDifference = std::remainder(mirrorYaw - facingYaw_, 2.0f * std::numbers::pi_v<float>);
	const float targetYaw = facingYaw_ + (isCarried
		? std::clamp(mirrorYawDifference * 0.45f, -0.40f, 0.40f) : 0.0f);
	const float targetPitch = isCarried
		? std::clamp(mirrorPitch * 0.14f, -0.25f, 0.25f) : 0.0f;
	const float blend = 1.0f - std::exp(-12.0f * frameTime);
	const float yawDifference = std::remainder(targetYaw - currentRotation.y, 2.0f * std::numbers::pi_v<float>);
	object_.SetRotate({
		currentRotation.x + (targetPitch - currentRotation.x) * blend,
		currentRotation.y + yawDifference * blend,
		0.0f,
	});
}

// 周期の端で速度が0になる余弦イージングを使い、移動とジャンプの切替も揺れ幅で補間します。
void Player::UpdateVisualFloat(float deltaTime, bool canFloat)
{
	const float frameTime = std::clamp(deltaTime, 0.0f, 0.1f);
	if (frameTime <= 0.0f) {
		return;
	}
	const bool isMoving = Length(moveDirection_) > 0.01f;
	const float period = isMoving ? kMovingFloatPeriod : kIdleFloatPeriod;
	const float targetAmplitude = canFloat
		? (isMoving ? kMovingFloatHeight : kIdleFloatHeight)
		: 0.0f;
	const float blend = 1.0f - std::exp(-kFloatBlendSpeed * frameTime);
	floatAmplitude_ += (targetAmplitude - floatAmplitude_) * blend;
	const float fullTurn = 2.0f * std::numbers::pi_v<float>;
	floatPhase_ = std::fmod(floatPhase_ + fullTurn * frameTime / period, fullTurn);
	floatOffsetY_ = floatAmplitude_ * (0.5f - 0.5f * std::cos(floatPhase_));
}

// モデルの拡縮やColliderを変えず、OBJの高さにふわふわ動く量だけを重ねます。
void Player::SyncVisualPosition()
{
	object_.SetTranslate({ position_.x, position_.y + floatOffsetY_, position_.z });
}
