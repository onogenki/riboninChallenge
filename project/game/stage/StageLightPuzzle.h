#pragma once

#include "Collision.h"
#include "Laser.h"
#include <memory>
#include <vector>

namespace Engine { class Camera; }
class DirectXCommon;
class FixedMirror;
class Object3d;
class LaserRenderer;

// Charge Laser・Door Laser・Switch・Doorの進行状態だけを担当するStage用ギミックです。
// 鏡やDoorモデルの寿命と描画はStage1が所有し、このクラスは受け取った非所有ポインタを更新するだけです。
class StageLightPuzzle
{
public:
	// forward宣言したLaserRendererを安全に生成・破棄するため、実装はcppに置きます。
	StageLightPuzzle();
	~StageLightPuzzle();

	// JSONやDebug UIから調整する、Laser・Switch・Doorの配置と判定値です。
	struct Settings
	{
		Vector3 laserOrigin{ 0.0f, 2.0f, 7.5f };
		Vector3 laserDirection{ 0.0f, -0.85f, -1.2f };
		Vector3 doorLaserOrigin{ -6.0f, 1.0f, 8.0f };
		Vector3 doorLaserDirection{ 1.0f, 0.0f, 0.0f };
		Vector3 chargeSwitchPosition{ 0.0f, 0.65f, 7.00f };
		float chargeSwitchRadius = 0.45f;
		Vector3 doorSwitchPosition{ -3.0f, 1.0f, 2.80f };
		float doorSwitchRadius = 0.80f;
		Vector3 doorClosedPosition{ -4.5f, -0.5f, 2.80f };
		Vector3 doorColliderLocalHalfSize{ 10.0f, 1.5f, 10.0f };
		float doorOpenHeight = 4.5f;
		float largeMirrorBaseYaw = 3.14159265f;
		float largeMirrorTargetYawOffset = 1.04719755f;
		float laserCollisionRadius = 0.06f;
		float laserVisualWidth = 0.32f;
	};

	// Laserの計算設定と描画用Rendererを準備し、光を描けない時は失敗を返します。
	bool Initialize(DirectXCommon* directXCommon);
	// Stageを切り替える時に、Rendererを作り直さずCharge・Door・Laserの進行状態だけを初期化します。
	void ResetProgress();
	// Scene終了時に、Puzzleが所有する描画用GPUデータを解放します。
	void Finalize();
	// 携帯鏡の更新後に、Laser・Switch・Door・大型Mirrorを一つの順番で更新します。
	void Update(
		float deltaTime,
		const std::vector<const Mirror*>& reflectors,
		const std::vector<MyMath::OBB>& blockingObbs,
		const MyMath::Sphere* playerSphere,
		FixedMirror* largeMirror,
		Object3d* laserEmitter,
		Object3d* doorLaserEmitter,
		Object3d* chargeSwitch,
		Object3d* doorSwitch,
		Object3d* lightDoor);

	// 描画とSpotLight化へ、更新済みの反射後Laser線分を渡します。
	const Laser& GetChargeLaser() const { return chargeLaser_; }
	const Laser& GetDoorLaser() const { return doorLaser_; }
	// Puzzleに属する二本のLaserを、通常CameraまたはMirror反射Cameraへ描画します。
	void Draw(const Engine::Camera& camera) const;
	// Player・Doorの判定やDebug表示へ、現在のギミック状態を返します。
	bool IsPlayerHitByLaser() const { return isPlayerHitByLaser_; }
	bool IsChargeSwitchReceivingLight() const { return isChargeSwitchReceivingLight_; }
	bool IsLargeMirrorCharged() const { return isLargeMirrorCharged_; }
	bool IsDoorSwitchReceivingLight() const { return isDoorSwitchReceivingLight_; }
	float GetMirrorCharge() const { return mirrorCharge_; }
	float GetLargeMirrorRotationAmount() const { return largeMirrorRotationAmount_; }
	float GetDoorOpenAmount() const { return doorOpenAmount_; }
	const MyMath::OBB& GetDoorCollider() const { return doorCollider_; }

	// 調整用の設定を読み取ります。変更はSetSettingsでRendererにも同時に反映します。
	const Settings& GetSettings() const { return settings_; }
	void SetSettings(const Settings& settings);

private:
	// Charge Laserの反射経路と、Player・Charge Switchへの接触を更新します。
	void UpdateChargeLaser(
		const std::vector<const Mirror*>& reflectors,
		const std::vector<MyMath::OBB>& blockingObbs,
		const MyMath::Sphere* playerSphere,
		Object3d* laserEmitter);
	// 充電量から大型Mirrorの向きを更新します。
	void UpdateLargeMirror(float deltaTime, FixedMirror& largeMirror);
	// Door Laserの反射経路と、Player・Door Switchへの接触を更新します。
	void UpdateDoorLaser(
		const std::vector<const Mirror*>& reflectors,
		const std::vector<MyMath::OBB>& blockingObbs,
		const MyMath::Sphere* playerSphere,
		Object3d* doorLaserEmitter);
	// Doorと二つのSwitchの見た目・Colliderを、現在の進行状態へ合わせます。
	void UpdatePuzzleVisuals(float deltaTime, Object3d* chargeSwitch, Object3d* doorSwitch, Object3d* lightDoor);

	Settings settings_{};
	Laser chargeLaser_{};
	Laser doorLaser_{};
	std::unique_ptr<LaserRenderer> chargeLaserRenderer_;
	std::unique_ptr<LaserRenderer> doorLaserRenderer_;
	MyMath::OBB doorCollider_{};
	bool isPlayerHitByLaser_ = false;
	bool isChargeSwitchReceivingLight_ = false;
	float mirrorCharge_ = 0.0f;
	bool isLargeMirrorCharged_ = false;
	float largeMirrorRotationAmount_ = 0.0f;
	bool isDoorSwitchReceivingLight_ = false;
	float doorOpenAmount_ = 0.0f;
};
