#include "StagePuzzleDebugUi.h"

#include "CarryableMirror.h"
#include "FixedMirror.h"
#include "ImGuiManager.h"
#include "MyMath.h"
#include "Player.h"
#include "StageCameraEvents.h"
#include "StageMapRuntime.h"

using namespace MyMath;

// 固定鏡のImGui操作を表示し、反射板とColliderの同期後にStage1へ保存準備を通知します。
void StagePuzzleDebugUi::DrawMirror(const MirrorContext& context)
{
	if (!context.fixedMirror) {
		return;
	}

	// UIは現在値のコピーを編集し、変更が確定した時だけ鏡の所有者へ渡します。
	Mirror mirror = context.fixedMirror->GetMirror();
	float mirrorYaw = context.fixedMirror->GetYaw();
	if (!ImGuiManager::GetInstance()->MirrorDebugWindow(
		mirror,
		mirrorYaw,
		context.fixedMirror->GetReflectionCamera(),
		context.fixedMirror->HasReflectionCapture())) {
		return;
	}

	context.fixedMirror->SetSurface(
		mirror.GetCenter(), mirrorYaw, mirror.GetWidth(), mirror.GetHeight());
	if (context.onMirrorChanged) {
		context.onMirrorChanged();
	}
}

// Light Puzzleの編集値を表示し、長さ0の方向ベクトルを安全な向きへ戻します。
bool StagePuzzleDebugUi::DrawLightPuzzle(const LightContext& context)
{
	if (!context.carryableMirror || !context.settings) {
		return false;
	}
	// 配置・方向・幅は設定として編集し、Puzzleの進行状態はコピーした値を表示します。
	StageLightPuzzle::Settings& settings = *context.settings;

	if (!ImGuiManager::GetInstance()->LightPuzzleDebugWindow(
		settings.laserOrigin,
		settings.laserDirection,
		settings.doorLaserOrigin,
		settings.doorLaserDirection,
		settings.laserVisualWidth,
		settings.chargeSwitchPosition,
		settings.doorSwitchPosition,
		settings.largeMirrorTargetYawOffset,
		context.carryableMirror->IsCarried(),
		context.isChargeSwitchReceivingLight,
		context.mirrorCharge,
		context.isLargeMirrorCharged,
		context.largeMirrorRotationAmount,
		context.isDoorSwitchReceivingLight,
		context.doorOpenAmount)) {
		return false;
	}

	if (Length(settings.laserDirection) > 0.0001f) {
		settings.laserDirection = Normalize(settings.laserDirection);
	} else {
		settings.laserDirection = { 0.0f, 0.0f, -1.0f };
	}
	if (Length(settings.doorLaserDirection) > 0.0001f) {
		settings.doorLaserDirection = Normalize(settings.doorLaserDirection);
	} else {
		settings.doorLaserDirection = { 1.0f, 0.0f, 0.0f };
	}
	return true;
}

// Stage1のCollider・Event・Camera位置を、Edit View用ワイヤーとして描画します。
void StagePuzzleDebugUi::DrawCollision(const CollisionContext& context)
{
	if (!context.player || !context.floorObb || !context.activeCamera ||
		!context.fixedMirrors || !context.stageMapRuntime || !context.stageCameraEvents) {
		return;
	}

	const Sphere playerSphere = context.player->GetSphere();
	const Collision::CollisionInfo floorCollision =
		context.player->CheckCollision(*context.floorObb);
	ImGuiManager::GetInstance()->DrawObbCollisionDebug(
		*context.floorObb,
		playerSphere,
		context.activeCamera,
		floorCollision.isCollision);

	for (const auto& fixedMirror : *context.fixedMirrors) {
		if (!fixedMirror) {
			continue;
		}
		const Collision::CollisionInfo mirrorCollision =
			context.player->CheckCollision(fixedMirror->GetCollider());
		ImGuiManager::GetInstance()->DrawObbCollisionDebug(
			fixedMirror->GetObb(),
			playerSphere,
			context.activeCamera,
			mirrorCollision.isCollision);
	}

	if (context.carryableMirror && !context.carryableMirror->IsCarried()) {
		const Collision::CollisionInfo carryableCollision =
			context.player->CheckCollision(context.carryableMirror->GetCollider());
		ImGuiManager::GetInstance()->DrawObbCollisionDebug(
			context.carryableMirror->GetObb(),
			playerSphere,
			context.activeCamera,
			carryableCollision.isCollision);
	}

	for (const StageMapRuntime::RuntimeObject& runtimeObject : context.stageMapRuntime->GetObjects()) {
		if (!runtimeObject.hasBoxCollider) {
			if (!runtimeObject.controlPoints.empty()) {
				ImGuiManager::GetInstance()->DrawControlPointPathDebug(
					runtimeObject.pathBasePosition,
					runtimeObject.controlPoints,
					context.activeCamera);
			}
			continue;
		}
		const Collision::CollisionInfo collision =
			context.player->CheckCollision(runtimeObject.GetCollider());
		ImGuiManager::GetInstance()->DrawObbCollisionDebug(
			runtimeObject.GetObb(),
			playerSphere,
			context.activeCamera,
			collision.isCollision);
		if (!runtimeObject.controlPoints.empty()) {
			ImGuiManager::GetInstance()->DrawControlPointPathDebug(
				runtimeObject.pathBasePosition,
				runtimeObject.controlPoints,
				context.activeCamera);
		}
	}

	for (const StageCameraEvents::EventTrigger& eventTrigger : context.stageCameraEvents->GetTriggers()) {
		ImGuiManager::GetInstance()->DrawObbCollisionDebug(
			eventTrigger.collider.GetShape(),
			playerSphere,
			context.activeCamera,
			eventTrigger.isPlayerInside);
	}

	for (const StageCameraEvents::EventCamera& eventCamera : context.stageCameraEvents->GetEventCameras()) {
		if (!eventCamera.camera) {
			continue;
		}
		const Transform cameraTransform{
			{ 1.0f, 1.0f, 1.0f },
			eventCamera.camera->GetRotate(),
			eventCamera.camera->GetTranslate(),
		};
		const OBB cameraDebugObb =
			Collision::MakeOBB(cameraTransform, { 0.25f, 0.25f, 0.25f });
		ImGuiManager::GetInstance()->DrawObbCollisionDebug(
			cameraDebugObb,
			playerSphere,
			context.activeCamera,
			eventCamera.sourceName == context.stageCameraEvents->GetActiveEventCameraName());
	}
}
