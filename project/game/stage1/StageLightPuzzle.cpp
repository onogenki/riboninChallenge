#include "StageLightPuzzle.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "FixedMirror.h"
#include "LaserRenderer.h"
#include "Object3d.h"
#include <algorithm>

using namespace MyMath;

StageLightPuzzle::StageLightPuzzle() = default;

StageLightPuzzle::~StageLightPuzzle() = default;

// Laserの最大距離・反射回数を、Stage1のPuzzle用初期値へそろえます。
void StageLightPuzzle::Initialize(DirectXCommon* directXCommon)
{
	// Sceneを再初期化しても前回の充電・Door状態を持ち越さないよう、実行中の値だけを戻します。
	chargeLaser_ = {};
	doorLaser_ = {};
	doorCollider_ = {};
	isPlayerHitByLaser_ = false;
	isChargeSwitchReceivingLight_ = false;
	mirrorCharge_ = 0.0f;
	isLargeMirrorCharged_ = false;
	largeMirrorRotationAmount_ = 0.0f;
	isDoorSwitchReceivingLight_ = false;
	doorOpenAmount_ = 0.0f;
	chargeLaser_.SetMaxDistance(30.0f);
	chargeLaser_.SetMaxReflectionCount(8);
	doorLaser_.SetMaxDistance(20.0f);
	doorLaser_.SetMaxReflectionCount(2);
	chargeLaserRenderer_ = std::make_unique<LaserRenderer>();
	if (!chargeLaserRenderer_->Initialize(directXCommon, 32)) {
		chargeLaserRenderer_.reset();
	} else {
		chargeLaserRenderer_->SetBeamWidth(settings_.laserVisualWidth);
		chargeLaserRenderer_->SetColor({ 0.05f, 0.95f, 1.00f, 1.0f });
	}
	doorLaserRenderer_ = std::make_unique<LaserRenderer>();
	if (!doorLaserRenderer_->Initialize(directXCommon, 8)) {
		doorLaserRenderer_.reset();
	} else {
		doorLaserRenderer_->SetBeamWidth(settings_.laserVisualWidth);
		doorLaserRenderer_->SetColor({ 1.00f, 0.38f, 0.05f, 1.0f });
	}
}

// Puzzleが所有するRendererを先に解放し、Sceneが次に初期化される準備をします。
void StageLightPuzzle::Finalize()
{
	chargeLaserRenderer_.reset();
	doorLaserRenderer_.reset();
}

// Charge/Door Laserを同じCameraへ描画し、通常画面とMirror反射画面の見た目をそろえます。
void StageLightPuzzle::Draw(const Camera& camera) const
{
	if (chargeLaserRenderer_) {
		chargeLaserRenderer_->Draw(chargeLaser_.GetSegments(), camera);
	}
	if (doorLaserRenderer_) {
		doorLaserRenderer_->Draw(doorLaser_.GetSegments(), camera);
	}
}

// Player・Mirror・Doorモデルを受け取り、Puzzleの一フレームを決まった順番で更新します。
void StageLightPuzzle::Update(
	float deltaTime,
	const std::vector<const Mirror*>& reflectors,
	const std::vector<OBB>& blockingObbs,
	const Sphere* playerSphere,
	FixedMirror* largeMirror,
	Object3d* laserEmitter,
	Object3d* doorLaserEmitter,
	Object3d* chargeSwitch,
	Object3d* doorSwitch,
	Object3d* lightDoor)
{
	if (!largeMirror || !chargeSwitch || !doorSwitch || !lightDoor) {
		return;
	}

	UpdateChargeLaser(reflectors, blockingObbs, playerSphere, laserEmitter);
	UpdateLargeMirror(deltaTime, *largeMirror);
	UpdateDoorLaser(reflectors, blockingObbs, playerSphere, doorLaserEmitter);
	UpdatePuzzleVisuals(deltaTime, chargeSwitch, doorSwitch, lightDoor);
}

// Charge Laserを更新し、Switchへ届く時間とPlayerへの接触を記録します。
void StageLightPuzzle::UpdateChargeLaser(
	const std::vector<const Mirror*>& reflectors,
	const std::vector<OBB>& blockingObbs,
	const Sphere* playerSphere,
	Object3d* laserEmitter)
{
	chargeLaser_.SetOrigin(settings_.laserOrigin);
	chargeLaser_.SetDirection(settings_.laserDirection);
	chargeLaser_.Update(reflectors, blockingObbs, settings_.laserVisualWidth * 0.5f);
	if (laserEmitter) {
		laserEmitter->SetTranslate(settings_.laserOrigin);
	}

	isPlayerHitByLaser_ =
		playerSphere && chargeLaser_.IsHitSphere(*playerSphere, settings_.laserCollisionRadius);
	const Sphere chargeSwitchSphere{
		settings_.chargeSwitchPosition,
		settings_.chargeSwitchRadius,
	};
	isChargeSwitchReceivingLight_ =
		chargeLaser_.IsHitSphere(chargeSwitchSphere, settings_.laserCollisionRadius);
}

// 充電量を大型Mirrorの横回転へ変換します。
void StageLightPuzzle::UpdateLargeMirror(float deltaTime, FixedMirror& largeMirror)
{
	if (!isLargeMirrorCharged_) {
		const float chargeTarget = isChargeSwitchReceivingLight_ ? 1.0f : 0.0f;
		const float chargeSpeed = isChargeSwitchReceivingLight_ ? 6.0f : 2.0f;
		const float chargeRate = (std::min)(chargeSpeed * (std::max)(deltaTime, 0.0f), 1.0f);
		mirrorCharge_ += (chargeTarget - mirrorCharge_) * chargeRate;
		if (mirrorCharge_ >= 0.98f) {
			mirrorCharge_ = 1.0f;
			isLargeMirrorCharged_ = true;
		}
	}

	const float mirrorRotationTarget = isLargeMirrorCharged_ ? 1.0f : 0.0f;
	const float mirrorRotationRate = (std::min)(1.8f * (std::max)(deltaTime, 0.0f), 1.0f);
	largeMirrorRotationAmount_ +=
		(mirrorRotationTarget - largeMirrorRotationAmount_) * mirrorRotationRate;
	largeMirror.SetPitch(0.0f);
	largeMirror.GetYawForEdit() =
		settings_.largeMirrorBaseYaw +
		settings_.largeMirrorTargetYawOffset * largeMirrorRotationAmount_;
	largeMirror.SyncVisualAndCollider();
}

// Door Laserを更新し、PlayerとDoor Switchへ届くかを記録します。
void StageLightPuzzle::UpdateDoorLaser(
	const std::vector<const Mirror*>& reflectors,
	const std::vector<OBB>& blockingObbs,
	const Sphere* playerSphere,
	Object3d* doorLaserEmitter)
{
	doorLaser_.SetOrigin(settings_.doorLaserOrigin);
	doorLaser_.SetDirection(settings_.doorLaserDirection);
	doorLaser_.Update(reflectors, blockingObbs, settings_.laserVisualWidth * 0.5f);
	if (doorLaserEmitter) {
		doorLaserEmitter->SetTranslate(settings_.doorLaserOrigin);
	}
	if (playerSphere) {
		isPlayerHitByLaser_ |= doorLaser_.IsHitSphere(*playerSphere, settings_.laserCollisionRadius);
	}
	const Sphere doorSwitchSphere{
		settings_.doorSwitchPosition,
		settings_.doorSwitchRadius,
	};
	isDoorSwitchReceivingLight_ =
		doorLaser_.IsHitSphere(doorSwitchSphere, settings_.laserCollisionRadius);
}

// Doorの開閉、Door Collider、Switchの大きさを一か所で同期します。
void StageLightPuzzle::UpdatePuzzleVisuals(
	float deltaTime,
	Object3d* chargeSwitch,
	Object3d* doorSwitch,
	Object3d* lightDoor)
{
	const float doorTarget = isDoorSwitchReceivingLight_ ? 1.0f : 0.0f;
	const float doorRate = (std::min)(3.0f * (std::max)(deltaTime, 0.0f), 1.0f);
	doorOpenAmount_ += (doorTarget - doorOpenAmount_) * doorRate;
	lightDoor->SetTranslate({
		settings_.doorClosedPosition.x,
		settings_.doorClosedPosition.y + settings_.doorOpenHeight * doorOpenAmount_,
		settings_.doorClosedPosition.z,
	});
	doorCollider_ = Collision::MakeOBB(
		lightDoor->GetTransform(),
		settings_.doorColliderLocalHalfSize);

	const float chargeSwitchScale = settings_.chargeSwitchRadius * 2.0f * (1.0f + mirrorCharge_ * 0.35f);
	chargeSwitch->SetTranslate(settings_.chargeSwitchPosition);
	chargeSwitch->SetScale({ chargeSwitchScale, chargeSwitchScale, chargeSwitchScale });
	const float doorSwitchScale = settings_.doorSwitchRadius * 2.0f *
		(isDoorSwitchReceivingLight_ ? 1.20f : 1.0f);
	doorSwitch->SetTranslate(settings_.doorSwitchPosition);
	doorSwitch->SetScale({ doorSwitchScale, doorSwitchScale, doorSwitchScale });
}
