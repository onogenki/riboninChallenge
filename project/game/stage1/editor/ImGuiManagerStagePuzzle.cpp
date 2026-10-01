#include "ImGuiManager.h"

#include "Camera.h"
#include "Mirror.h"
#include <algorithm>
#include <cmath>

// Stage1の環境光とキー・フィル・バックライトを、実行中に調整するUIです。
bool ImGuiManager::StageLightingWindow(LevelLoader::LightingData& lighting)
{
	bool isChanged = false;
#ifdef USE_IMGUI
	if (inspectorDockId_ != 0) {
		// 古いレイアウトに浮動位置が残っていても、ほかのStage編集タブを覆わないようにします。
		ImGui::SetNextWindowDockID(inspectorDockId_, ImGuiCond_Always);
	}
	if (!ImGui::Begin("Stage Lighting")) {
		ImGui::End();
		return false;
	}

	ImGui::TextUnformatted("Adjust the room brightness, then press Save Map in Stage Map Editor.");
	ImGui::SeparatorText("Overall Light");
	isChanged |= ImGui::ColorEdit3("Directional: color", &lighting.directionalColor.x);
	isChanged |= ImGui::DragFloat3("Directional: direction", &lighting.directionalDirection.x, 0.01f);
	isChanged |= ImGui::DragFloat("Directional: intensity", &lighting.directionalIntensity, 0.01f, 0.0f, 5.0f);
	isChanged |= ImGui::ColorEdit3("Ambient: color", &lighting.ambientColor.x);
	isChanged |= ImGui::DragFloat("Ambient: intensity", &lighting.ambientIntensity, 0.01f, 0.0f, 1.0f);
	isChanged |= ImGui::ColorEdit3("Point: color", &lighting.pointColor.x);
	isChanged |= ImGui::DragFloat3("Point: position", &lighting.pointPosition.x, 0.05f);
	isChanged |= ImGui::DragFloat("Point: intensity", &lighting.pointIntensity, 0.01f, 0.0f, 10.0f);
	isChanged |= ImGui::DragFloat("Point: radius", &lighting.pointRadius, 0.1f, 0.1f, 200.0f);
	isChanged |= ImGui::DragFloat("Point: decay", &lighting.pointDecay, 0.01f, 0.01f, 5.0f);

	ImGui::SeparatorText("Three-Point Lighting");
	for (size_t index = 0; index < lighting.spotLights.size(); ++index) {
		LevelLoader::SpotLightData& spotLight = lighting.spotLights[index];
		ImGui::PushID(static_cast<int>(index));
		if (ImGui::TreeNode(spotLight.name.c_str())) {
			isChanged |= ImGui::ColorEdit3("Color", &spotLight.color.x);
			isChanged |= ImGui::DragFloat3("Position", &spotLight.position.x, 0.05f);
			isChanged |= ImGui::DragFloat3("Direction", &spotLight.direction.x, 0.01f);
			isChanged |= ImGui::DragFloat("Intensity", &spotLight.intensity, 0.01f, 0.0f, 10.0f);
			isChanged |= ImGui::DragFloat("Distance", &spotLight.distance, 0.1f, 0.1f, 200.0f);
			isChanged |= ImGui::DragFloat("Decay", &spotLight.decay, 0.01f, 0.01f, 5.0f);
			isChanged |= ImGui::DragFloat("Cos Angle", &spotLight.cosAngle, 0.01f, 0.0f, 0.99f);
			isChanged |= ImGui::DragFloat("Cos Falloff Start", &spotLight.cosFalloffStart, 0.01f, 0.0f, 1.0f);
			if (spotLight.cosFalloffStart < spotLight.cosAngle) {
				// 円錐の中心側から減衰が始まるよう、開始値が外側の角度より小さくならないようにします。
				spotLight.cosFalloffStart = spotLight.cosAngle;
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	ImGui::End();
#else
	(void)lighting;
#endif
	return isChanged;
}

// 鏡の中心・大きさ・回転を編集するデバッグ用ウィンドウを表示します。
bool ImGuiManager::MirrorDebugWindow(
	Mirror& mirror,
	float& mirrorYaw,
	const Camera& reflectionCamera,
	bool hasReflectionCapture)
{
#ifdef USE_IMGUI
	if (inspectorDockId_ != 0) {
		ImGui::SetNextWindowDockID(inspectorDockId_, ImGuiCond_Always);
	}
	if (!ImGui::Begin("Inspector", &showModelWindow_)) {
		ImGui::End();
		return false;
	}

	ImGui::Text("Planar Reflection Mirror");
	if (hasReflectionCapture) {
		ImGui::TextColored(
			ImVec4(0.35f, 1.0f, 0.55f, 1.0f),
			"Reflection texture: ACTIVE");
		ImGui::TextWrapped("The room is being drawn by the reflection camera and shown on this mirror.");
	} else {
		ImGui::TextColored(
			ImVec4(1.0f, 0.85f, 0.25f, 1.0f),
			"Reflection texture: waiting for first capture");
	}

	bool isChanged = false;
	Vector3 center = mirror.GetCenter();
	if (ImGui::DragFloat3("Mirror Center", &center.x, 0.05f)) {
		mirror.SetCenter(center);
		isChanged = true;
	}

	float width = mirror.GetWidth();
	float height = mirror.GetHeight();
	if (ImGui::DragFloat("Mirror Width", &width, 0.05f, 0.01f, 20.0f) |
		ImGui::DragFloat("Mirror Height", &height, 0.05f, 0.01f, 20.0f)) {
		mirror.SetSize(width, height);
		isChanged = true;
	}

	if (ImGui::SliderAngle("Mirror Y Rotation", &mirrorYaw, -180.0f, 180.0f)) {
		// plane.objの正面はローカル座標の+Z方向です。
		mirror.SetNormal({ std::sin(mirrorYaw), 0.0f, std::cos(mirrorYaw) });
		isChanged = true;
	}

	const Vector3& normal = mirror.GetNormal();
	ImGui::Text("Mirror Normal: %.2f, %.2f, %.2f", normal.x, normal.y, normal.z);
	ImGui::Separator();
	const Vector3& reflectionPosition = reflectionCamera.GetTranslate();
	const Vector3& reflectionRotate = reflectionCamera.GetRotate();
	ImGui::Text("Reflection Camera Position: %.2f, %.2f, %.2f", reflectionPosition.x, reflectionPosition.y, reflectionPosition.z);
	ImGui::Text("Reflection Camera Rotation: %.2f, %.2f, %.2f", reflectionRotate.x, reflectionRotate.y, reflectionRotate.z);
	ImGui::TextWrapped("This camera is mirrored across the plane and draws the room into the mirror texture.");
	ImGui::End();
	return isChanged;
#else
	(void)mirror;
	(void)mirrorYaw;
	(void)reflectionCamera;
	(void)hasReflectionCapture;
	return false;
#endif
}

// 反射Laserの発射位置・方向と、Switch・Doorの進行状態を表示します。
bool ImGuiManager::LightPuzzleDebugWindow(
	Vector3& laserOrigin,
	Vector3& laserDirection,
	Vector3& doorLaserOrigin,
	Vector3& doorLaserDirection,
	float& laserVisualWidth,
	Vector3& chargeSwitchPosition,
	Vector3& doorSwitchPosition,
	float& largeMirrorTargetYawOffset,
	bool isMirrorCarried,
	bool isChargeSwitchReceivingLight,
	float mirrorCharge,
	bool isLargeMirrorCharged,
	float largeMirrorRotationAmount,
	bool isDoorSwitchReceivingLight,
	float doorOpenAmount)
{
#ifdef USE_IMGUI
	if (inspectorDockId_ != 0) {
		ImGui::SetNextWindowDockID(inspectorDockId_, ImGuiCond_Always);
	}
	if (!ImGui::Begin("Light Reflection Puzzle", &showModelWindow_)) {
		ImGui::End();
		return false;
	}

	ImGui::Text("Laser Source Settings");
	ImGui::TextWrapped("The cyan Charge Laser is reflected by the carried mirror. The orange Door Laser is reflected by the large mirror after its 90 degree turn.");
	bool isChanged = false;
	if (ImGui::DragFloat3("Charge Laser Origin", &laserOrigin.x, 0.05f)) {
		isChanged = true;
	}
	if (ImGui::DragFloat3("Charge Laser Direction", &laserDirection.x, 0.02f)) {
		isChanged = true;
	}
	if (ImGui::DragFloat3("Door Laser Origin", &doorLaserOrigin.x, 0.05f)) {
		isChanged = true;
	}
	if (ImGui::DragFloat3("Door Laser Direction", &doorLaserDirection.x, 0.02f)) {
		isChanged = true;
	}
	if (ImGui::SliderFloat("Laser Visual Width", &laserVisualWidth, 0.04f, 0.80f)) {
		isChanged = true;
	}
	if (ImGui::DragFloat3("Charge Switch Position", &chargeSwitchPosition.x, 0.05f)) {
		isChanged = true;
	}
	if (ImGui::DragFloat3("Door Switch Position", &doorSwitchPosition.x, 0.05f)) {
		isChanged = true;
	}
	if (ImGui::SliderAngle("Large Mirror Yaw Offset", &largeMirrorTargetYawOffset, 0.0f, 90.0f)) {
		isChanged = true;
	}

	ImGui::Separator();
	ImGui::Text("Puzzle Status: Step 1 - Charge Large Mirror");
	ImGui::Text("Carry Mirror: %s", isMirrorCarried ? "CARRIED" : "ON FLOOR (Press E nearby)");
	ImGui::TextColored(
		isChargeSwitchReceivingLight
			? ImVec4(1.0f, 0.85f, 0.20f, 1.0f)
			: ImVec4(0.70f, 0.70f, 0.70f, 1.0f),
		"Charge Light: %s",
		isChargeSwitchReceivingLight ? "HITTING SWITCH" : "NOT HITTING");
	ImGui::ProgressBar(
		std::clamp(mirrorCharge, 0.0f, 1.0f),
		ImVec2(-1.0f, 0.0f),
		"Mirror Charge");
	ImGui::Text("Large Mirror: %s (%.0f%% rotated)",
		isLargeMirrorCharged ? "CHARGED" : "WAITING FOR CHARGE",
		std::clamp(largeMirrorRotationAmount, 0.0f, 1.0f) * 100.0f);

	ImGui::Separator();
	ImGui::Text("Puzzle Status: Step 2 - Open Door");
	ImGui::TextColored(
		isDoorSwitchReceivingLight
			? ImVec4(0.35f, 1.0f, 0.55f, 1.0f)
			: ImVec4(0.70f, 0.70f, 0.70f, 1.0f),
		"Large Mirror Reflection: %s",
		isDoorSwitchReceivingLight ? "HITTING DOOR SWITCH" : "NOT HITTING");
	ImGui::Text("Door: %s (%.0f%%)",
		isDoorSwitchReceivingLight ? "OPENING / OPEN" : "CLOSING / CLOSED",
		std::clamp(doorOpenAmount, 0.0f, 1.0f) * 100.0f);
	ImGui::TextWrapped("The large mirror always reflects on its front side. Charge it to swing the mirror sideways and redirect the reflected light to the door switch.");
	ImGui::End();
	return isChanged;
#else
	(void)laserOrigin;
	(void)laserDirection;
	(void)doorLaserOrigin;
	(void)doorLaserDirection;
	(void)laserVisualWidth;
	(void)chargeSwitchPosition;
	(void)doorSwitchPosition;
	(void)largeMirrorTargetYawOffset;
	(void)isMirrorCarried;
	(void)isChargeSwitchReceivingLight;
	(void)mirrorCharge;
	(void)isLargeMirrorCharged;
	(void)largeMirrorRotationAmount;
	(void)isDoorSwitchReceivingLight;
	(void)doorOpenAmount;
	return false;
#endif
}
