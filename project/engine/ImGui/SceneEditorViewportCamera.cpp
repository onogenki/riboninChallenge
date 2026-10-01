#include "SceneEditor.h"

#include "Camera.h"
#include "ImGuiManager.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#endif

// Game View上の右・中ドラッグとホイールで、編集対象のCameraを安全に操作します。
void SceneEditor::UpdateViewportCamera(Camera* camera, bool inputBlocked)
{
#ifdef USE_IMGUI
	if (!camera || inputBlocked) {
		return;
	}
	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}
	const ImRect imageRect(
		ImVec2(rectX, rectY),
		ImVec2(rectX + rectWidth, rectY + rectHeight));
	if (!imageRect.Contains(ImGui::GetMousePos()) || ImGui::IsAnyItemActive()) {
		return;
	}

	Vector3 cameraPosition = camera->GetTranslate();
	Vector3 cameraRotation = camera->GetRotate();
	const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
	bool cameraChanged = false;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		cameraRotation.y += mouseDelta.x * 0.005f;
		cameraRotation.x = std::clamp(cameraRotation.x + mouseDelta.y * 0.005f, -1.45f, 1.45f);
		cameraChanged = true;
	}
	if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		const Vector3 right{ std::cos(cameraRotation.y), 0.0f, -std::sin(cameraRotation.y) };
		cameraPosition.x -= right.x * mouseDelta.x * 0.01f;
		cameraPosition.z -= right.z * mouseDelta.x * 0.01f;
		cameraPosition.y += mouseDelta.y * 0.01f;
		cameraChanged = true;
	}
	if (std::abs(ImGui::GetIO().MouseWheel) > 0.0001f) {
		const float cosinePitch = std::cos(cameraRotation.x);
		const Vector3 forward{
			std::sin(cameraRotation.y) * cosinePitch,
			-std::sin(cameraRotation.x),
			std::cos(cameraRotation.y) * cosinePitch,
		};
		const float zoomAmount = ImGui::GetIO().MouseWheel * 0.6f;
		cameraPosition.x += forward.x * zoomAmount;
		cameraPosition.y += forward.y * zoomAmount;
		cameraPosition.z += forward.z * zoomAmount;
		cameraChanged = true;
	}
	if (cameraChanged) {
		camera->SetTranslate(cameraPosition);
		camera->SetRotate(cameraRotation);
	}
#else
	(void)camera;
	(void)inputBlocked;
#endif
}
