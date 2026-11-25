#pragma once

#include "Collision.h"

namespace Engine { class DirectXCommon; }
class FixedMirror;
namespace Engine { class Object3d; }
class StageHazardLights;

// 光を受けるSwitch・Mirror・Doorの進行状態を担当するStage用ギミックです。
// LightそのものはStageHazardLightsが所有し、このクラスは受け取った判定結果を使います。
class StageLightPuzzle
{
public:
	StageLightPuzzle();
	~StageLightPuzzle();

	// Switch・Doorと動く鏡の配置や判定値です。
	struct Settings
	{
		Vector3 chargeSwitchPosition{ 0.0f, 0.65f, 7.00f };
		float chargeSwitchRadius = 0.45f;
		Vector3 doorSwitchPosition{ -3.0f, 1.0f, 2.80f };
		float doorSwitchRadius = 0.80f;
		Vector3 doorClosedPosition{ -4.5f, -0.5f, 2.80f };
		// wall.objの元の半寸法に合わせ、見えない床中央まで扉の判定を広げません。
		Vector3 doorColliderLocalHalfSize{ 2.0f, 2.0f, 0.7f };
		float doorOpenHeight = 4.5f;
		float largeMirrorBaseYaw = 3.14159265f;
		float largeMirrorTargetYawOffset = 1.04719755f;
	};

	// Stage開始時に、前回のSwitch・Door進行状態を戻します。
	bool Initialize(Engine::DirectXCommon* directXCommon);
	// Stageを切り替える時に、SwitchとDoorの進行状態を初期化します。
	void ResetProgress();
	// 反射後のLightを受け、Switch・Door・大型Mirrorを一つの順番で更新します。
	void Update(
		float deltaTime,
		const StageHazardLights& hazardLights,
		FixedMirror* largeMirror,
		Engine::Object3d* chargeSwitch,
		Engine::Object3d* doorSwitch,
		Engine::Object3d* lightDoor);

	// Player・Doorの判定やDebug表示へ、現在のギミック状態を返します。
	bool IsChargeSwitchReceivingLight() const { return isChargeSwitchReceivingLight_; }
	bool IsLargeMirrorCharged() const { return isLargeMirrorCharged_; }
	bool IsDoorSwitchReceivingLight() const { return isDoorSwitchReceivingLight_; }
	float GetMirrorCharge() const { return mirrorCharge_; }
	float GetLargeMirrorRotationAmount() const { return largeMirrorRotationAmount_; }
	float GetDoorOpenAmount() const { return doorOpenAmount_; }
	const MyMath::OBB& GetDoorCollider() const { return doorCollider_; }

	// 調整用の設定を読み取り、変更はSetSettingsで確定します。
	const Settings& GetSettings() const { return settings_; }
	void SetSettings(const Settings& settings);

private:
	// 充電量から大型Mirrorの向きを更新します。
	void UpdateLargeMirror(float deltaTime, FixedMirror& largeMirror);
	// Doorと二つのSwitchの見た目・Colliderを、現在の進行状態へ合わせます。
	void UpdatePuzzleVisuals(float deltaTime, Engine::Object3d* chargeSwitch, Engine::Object3d* doorSwitch, Engine::Object3d* lightDoor);

	Settings settings_{};
	MyMath::OBB doorCollider_{};
	bool isChargeSwitchReceivingLight_ = false;
	float mirrorCharge_ = 0.0f;
	bool isLargeMirrorCharged_ = false;
	float largeMirrorRotationAmount_ = 0.0f;
	bool isDoorSwitchReceivingLight_ = false;
	float doorOpenAmount_ = 0.0f;
};
