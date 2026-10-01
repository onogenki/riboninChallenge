#include "DebugGameViewCameraController.h"

#include "Camera.h"
#include "DebugAssetPreview.h"
#include "ImGuiManager.h"
#include "Input.h"
#include <algorithm>

// Edit View・Preview状態を考慮しながら、Game ViewのCamera移動・回転・拡縮を更新します。
void DebugGameViewCameraController::Update(
	Camera* activeCamera,
	DebugAssetPreview& assetPreview)
{
	if (!activeCamera) {
		return;
	}

	const bool isEditView = ImGuiManager::GetInstance()->IsEditViewActive();
	if (isEditView && !assetPreview.IsActive()) {
		// Edit ViewのCameraは共通SceneEditorが担当し、二重操作を防ぎます。
		isDragging_ = false;
		return;
	}

	Input* input = Input::GetInstance();
	const Vector2 mouseScreen = input->GetMouseScreen();
	const bool isMouseOverGameView = ImGuiManager::GetInstance()->IsMouseOverGameView(
		mouseScreen.x,
		mouseScreen.y);
	const bool isMouseButtonDown =
		(!isEditView && input->IsMouseButtonPressed(0)) ||
		input->IsMouseButtonPressed(1) ||
		input->IsMouseButtonPressed(2);
	if (!isMouseButtonDown) {
		isDragging_ = false;
	}

	const bool startedAllowedCameraButton =
		(!isEditView && input->TriggerMouseButton(0)) ||
		input->TriggerMouseButton(1) ||
		input->TriggerMouseButton(2);
	if (isMouseOverGameView && startedAllowedCameraButton) {
		isDragging_ = true;
	}

	if (assetPreview.UpdateInput(activeCamera, input, isMouseOverGameView, isDragging_)) {
		return;
	}

	constexpr float kMoveSpeed = 0.01f;
	constexpr float kRotateSpeed = 0.005f;
	constexpr float kZoomSpeed = 0.01f;
	constexpr float kMinimumPitch = -1.45f;
	constexpr float kMaximumPitch = 1.45f;
	Vector3 cameraTranslate = activeCamera->GetTranslate();
	Vector3 cameraRotate = activeCamera->GetRotate();

	if (!isEditView && isDragging_ && input->IsMouseButtonPressed(0)) {
		cameraTranslate.x -= static_cast<float>(input->GetMouseX()) * kMoveSpeed;
		cameraTranslate.y += static_cast<float>(input->GetMouseY()) * kMoveSpeed;
	}
	if (isDragging_ && (input->IsMouseButtonPressed(1) || input->IsMouseButtonPressed(2))) {
		cameraRotate.y += static_cast<float>(input->GetMouseX()) * kRotateSpeed;
		cameraRotate.x += static_cast<float>(input->GetMouseY()) * kRotateSpeed;
		cameraRotate.x = std::clamp(cameraRotate.x, kMinimumPitch, kMaximumPitch);
	}
	if (isMouseOverGameView && input->GetMouseWheel() != 0) {
		cameraTranslate.z += static_cast<float>(input->GetMouseWheel()) * kZoomSpeed;
	}

	activeCamera->SetTranslate(cameraTranslate);
	activeCamera->SetRotate(cameraRotate);
}
