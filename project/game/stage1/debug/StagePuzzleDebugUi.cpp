#include "StagePuzzleDebugUi.h"

#include "CarryableMirror.h"
#include "FixedMirror.h"
#include "ImGuiManager.h"
#include "LaserRenderer.h"
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

	Mirror& mirror = context.fixedMirror->GetMirror();
	if (!ImGuiManager::GetInstance()->MirrorDebugWindow(
		mirror,
		context.fixedMirror->GetYawForEdit(),
		context.fixedMirror->GetReflectionCamera(),
		context.fixedMirror->HasReflectionCapture())) {
		return;
	}

	context.fixedMirror->SyncVisualAndCollider();
	if (context.onMirrorChanged) {
		context.onMirrorChanged();
	}
}

// Light Puzzleの編集値を表示し、長さ0の方向ベクトルを安全な向きへ戻します。
void StagePuzzleDebugUi::DrawLightPuzzle(const LightContext& context)
{
	if (!context.carryableMirror || !context.laserOrigin || !context.laserDirection ||
		!context.doorLaserOrigin || !context.doorLaserDirection || !context.laserVisualWidth ||
		!context.chargeSwitchPosition || !context.doorSwitchPosition ||
		!context.largeMirrorTargetYawOffset || !context.isChargeSwitchReceivingLight ||
		!context.mirrorCharge || !context.isLargeMirrorCharged ||
		!context.largeMirrorRotationAmount || !context.isDoorSwitchReceivingLight ||
		!context.doorOpenAmount) {
		return;
	}

	if (!ImGuiManager::GetInstance()->LightPuzzleDebugWindow(
		*context.laserOrigin,
		*context.laserDirection,
		*context.doorLaserOrigin,
		*context.doorLaserDirection,
		*context.laserVisualWidth,
		*context.chargeSwitchPosition,
		*context.doorSwitchPosition,
		*context.largeMirrorTargetYawOffset,
		context.carryableMirror->IsCarried(),
		*context.isChargeSwitchReceivingLight,
		*context.mirrorCharge,
		*context.isLargeMirrorCharged,
		*context.largeMirrorRotationAmount,
		*context.isDoorSwitchReceivingLight,
		*context.doorOpenAmount)) {
		return;
	}

	if (Length(*context.laserDirection) > 0.0001f) {
		*context.laserDirection = Normalize(*context.laserDirection);
	} else {
		*context.laserDirection = { 0.0f, 0.0f, -1.0f };
	}
	if (Length(*context.doorLaserDirection) > 0.0001f) {
		*context.doorLaserDirection = Normalize(*context.doorLaserDirection);
	} else {
		*context.doorLaserDirection = { 1.0f, 0.0f, 0.0f };
	}
	if (context.laserRenderer) {
		context.laserRenderer->SetBeamWidth(*context.laserVisualWidth);
	}
	if (context.doorLaserRenderer) {
		context.doorLaserRenderer->SetBeamWidth(*context.laserVisualWidth);
	}
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
