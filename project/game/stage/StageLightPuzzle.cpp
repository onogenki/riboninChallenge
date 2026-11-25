#include "StageLightPuzzle.h"

#include "DirectXCommon.h"
#include "FixedMirror.h"
#include "Object3d.h"
#include "StageHazardLights.h"
#include <algorithm>

using namespace MyMath;

StageLightPuzzle::StageLightPuzzle() = default;

StageLightPuzzle::~StageLightPuzzle() = default;

// Stage開始時に前回のSwitch・Door状態を消し、描画基盤がない場合は失敗します。
bool StageLightPuzzle::Initialize(Engine::DirectXCommon* directXCommon)
{
	// Sceneを再初期化しても前回の充電・Door状態を持ち越さないよう、実行中の値だけを戻します。
	ResetProgress();
	return directXCommon != nullptr;
}

// Stage間でPuzzleの進行状態を持ち越さないよう、実行値を初期化します。
void StageLightPuzzle::ResetProgress()
{
	doorCollider_ = {};
	isChargeSwitchReceivingLight_ = false;
	mirrorCharge_ = 0.0f;
	isLargeMirrorCharged_ = false;
	largeMirrorRotationAmount_ = 0.0f;
	isDoorSwitchReceivingLight_ = false;
	doorOpenAmount_ = 0.0f;
}

// Switch・Doorの調整値をPuzzleへ確定します。
void StageLightPuzzle::SetSettings(const Settings& settings)
{
	settings_ = settings;
}

// 反射後のLightとMirror・Doorモデルを受け、Puzzleの一フレームを更新します。
void StageLightPuzzle::Update(
	float deltaTime,
	const StageHazardLights& hazardLights,
	FixedMirror* largeMirror,
	Engine::Object3d* chargeSwitch,
	Engine::Object3d* doorSwitch,
	Engine::Object3d* lightDoor)
{
	if (!largeMirror || !chargeSwitch || !doorSwitch || !lightDoor) {
		return;
	}

	// 光源の種類に関係なく、同じフレームの反射後の光が両方のSwitchへ届くか調べます。
	const Sphere chargeSwitchSphere{ settings_.chargeSwitchPosition, settings_.chargeSwitchRadius };
	const Sphere doorSwitchSphere{ settings_.doorSwitchPosition, settings_.doorSwitchRadius };
	const auto receivesLight = [this, &hazardLights](const Sphere& sphere)
	{
		return hazardLights.IsHitSphere(sphere);
	};
	isChargeSwitchReceivingLight_ = receivesLight(chargeSwitchSphere);
	isDoorSwitchReceivingLight_ = receivesLight(doorSwitchSphere);
	UpdateLargeMirror(deltaTime, *largeMirror);
	UpdatePuzzleVisuals(deltaTime, chargeSwitch, doorSwitch, lightDoor);
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
	largeMirror.SetOrientation(
		0.0f,
		settings_.largeMirrorBaseYaw +
		settings_.largeMirrorTargetYawOffset * largeMirrorRotationAmount_);
}

// Doorの開閉、Door Collider、Switchの大きさを一か所で同期します。
void StageLightPuzzle::UpdatePuzzleVisuals(
	float deltaTime,
	Engine::Object3d* chargeSwitch,
	Engine::Object3d* doorSwitch,
	Engine::Object3d* lightDoor)
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
