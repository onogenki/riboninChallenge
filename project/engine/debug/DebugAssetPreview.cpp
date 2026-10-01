#include "DebugAssetPreview.h"

#include "Sprite.h"
#include "SpriteCommon.h"
#include "Camera.h"
#include "Object3d.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "../ImGui/ImGuiManager.h"
#include <algorithm>
#include <cmath>
#include <dinput.h>

// プレビューへ入る直前のCamera状態を、一度だけ保存します。
void DebugAssetPreview::SaveReturnCamera(Camera* camera)
{
	if (camera && !isActive_) {
		returnCameraTranslate_ = camera->GetTranslate();
		returnCameraRotate_ = camera->GetRotate();
	}
}

// 作成済みモデルをプレビューへ移し、現在のCamera位置を復帰用として保存します。
bool DebugAssetPreview::EnterModel(
	std::unique_ptr<Object3d> object,
	const std::string& fileName,
	const Vector3& modelCenter,
	float modelRadius,
	Camera* camera)
{
	if (!object) {
		return false;
	}

	SaveReturnCamera(camera);
	object->SetTranslate({ -modelCenter.x, -modelCenter.y, -modelCenter.z });
	object->SetScale({ 1.0f, 1.0f, 1.0f });
	object_ = std::move(object);
	sprite_.reset();
	displayName_ = fileName;
	textureFilePath_.clear();
	isActive_ = true;
	isTexturePreview_ = false;
	suppressExitUntilMouseRelease_ = true;
	defaultCameraDistance_ = (std::max)(3.0f, modelRadius * 3.2f);
	ResetModelCamera(camera);
	return true;
}

// TextureからSpriteを作成し、現在のCamera位置を復帰用として保存します。
bool DebugAssetPreview::EnterTexture(
	SpriteCommon* spriteCommon,
	const std::string& textureFilePath,
	Camera* camera)
{
	if (!spriteCommon || textureFilePath.empty()) {
		return false;
	}

	SaveReturnCamera(camera);
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon, textureFilePath);
	sprite->SetAnchorPoint({ 0.5f, 0.5f });
	object_.reset();
	sprite_ = std::move(sprite);
	displayName_ = textureFilePath;
	textureFilePath_ = textureFilePath;
	isActive_ = true;
	isTexturePreview_ = true;
	suppressExitUntilMouseRelease_ = true;
	ResetTextureView();
	return true;
}

// プレビューを終了し、開始前のCamera位置・回転を戻します。
void DebugAssetPreview::Exit(Camera* camera)
{
	if (!isActive_) {
		return;
	}
	if (camera) {
		camera->SetTranslate(returnCameraTranslate_);
		camera->SetRotate(returnCameraRotate_);
	}
	Clear();
}

// Scene終了時に、Cameraへ触れずプレビュー所有物だけを解放します。
void DebugAssetPreview::Clear()
{
	object_.reset();
	sprite_.reset();
	displayName_.clear();
	textureFilePath_.clear();
	cameraTarget_ = {};
	cameraDistance_ = 3.0f;
	defaultCameraDistance_ = 3.0f;
	cameraYaw_ = 0.0f;
	cameraPitch_ = 0.0f;
	isActive_ = false;
	isTexturePreview_ = false;
	suppressExitUntilMouseRelease_ = false;
}

// Textureの縦横比を保ちつつ、Game View内へ収まる大きさと位置へ戻します。
void DebugAssetPreview::ResetTextureView()
{
	if (!sprite_) {
		return;
	}
	const float clientWidth = static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight());
	const Vector2 originalSize = sprite_->GetTextureSize();
	const float maxWidth = (std::max)(160.0f, clientWidth * 0.72f);
	const float maxHeight = (std::max)(120.0f, clientHeight * 0.72f);
	const float scale = (std::min)(
		maxWidth / (std::max)(originalSize.x, 1.0f),
		maxHeight / (std::max)(originalSize.y, 1.0f));
	sprite_->SetSize({ originalSize.x * scale, originalSize.y * scale });
	sprite_->SetPosition({ clientWidth * 0.5f, clientHeight * 0.5f });
}

// モデル中心を注視する初期Camera位置・回転へ戻します。
void DebugAssetPreview::ResetModelCamera(Camera* camera)
{
	if (!isActive_ || isTexturePreview_) {
		return;
	}
	cameraTarget_ = { 0.0f, 0.0f, 0.0f };
	cameraYaw_ = 0.0f;
	cameraPitch_ = 0.0f;
	cameraDistance_ = defaultCameraDistance_;
	if (camera) {
		camera->SetTranslate({ 0.0f, 0.0f, -cameraDistance_ });
		camera->SetRotate({ 0.0f, 0.0f, 0.0f });
	}
}

// モデル用Camera、またはTextureの表示位置・大きさを初期値へ戻します。
void DebugAssetPreview::Reset(Camera* camera)
{
	if (!isActive_) {
		return;
	}
	if (isTexturePreview_) {
		ResetTextureView();
	} else {
		ResetModelCamera(camera);
	}
}

// プレビュー中のMouse操作とRキー操作を更新します。プレビュー中ならtrueを返します。
bool DebugAssetPreview::UpdateInput(
	Camera* camera,
	Input* input,
	bool isMouseOverGameView,
	bool isCameraDragging)
{
	if (!isActive_) {
		return false;
	}
	if (!camera || !input) {
		return true;
	}

	constexpr float rotateSpeed = 0.005f;
	constexpr float zoomSpeed = 0.01f;
	constexpr float minPitch = -1.45f;
	constexpr float maxPitch = 1.45f;
	if (isMouseOverGameView && input->TriggerKey(DIK_R)) {
		Reset(camera);
	}

	if (isTexturePreview_) {
		float rectX = 0.0f;
		float rectY = 0.0f;
		float rectWidth = 0.0f;
		float rectHeight = 0.0f;
		if (sprite_ && ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight) &&
			rectWidth > 0.0f && rectHeight > 0.0f) {
			const float clientWidth = static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth());
			const float clientHeight = static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight());
			if (isCameraDragging && input->IsMouseButtonPressed(0)) {
				Vector2 position = sprite_->GetPosition();
				position.x += static_cast<float>(input->GetMouseX()) * (clientWidth / rectWidth);
				position.y += static_cast<float>(input->GetMouseY()) * (clientHeight / rectHeight);
				sprite_->SetPosition(position);
			}
			if (isMouseOverGameView && input->GetMouseWheel() != 0) {
				Vector2 size = sprite_->GetSize();
				const float zoomFactor = std::clamp(
					1.0f + static_cast<float>(input->GetMouseWheel()) * 0.001f,
					0.2f,
					4.0f);
				size.x = std::clamp(size.x * zoomFactor, 8.0f, clientWidth * 4.0f);
				size.y = std::clamp(size.y * zoomFactor, 8.0f, clientHeight * 4.0f);
				sprite_->SetSize(size);
			}
			sprite_->Update();
		}
		return true;
	}

	const float targetMoveSpeed = (std::max)(0.003f, cameraDistance_ * 0.0025f);
	if (isCameraDragging && input->IsMouseButtonPressed(0)) {
		const float yawCos = std::cos(cameraYaw_);
		const float yawSin = std::sin(cameraYaw_);
		const float pitchCos = std::cos(cameraPitch_);
		const float pitchSin = std::sin(cameraPitch_);
		const Vector3 right{ yawCos, 0.0f, -yawSin };
		const Vector3 up{ yawSin * pitchSin, pitchCos, yawCos * pitchSin };
		const float mouseX = static_cast<float>(input->GetMouseX());
		const float mouseY = static_cast<float>(input->GetMouseY());
		cameraTarget_.x -= right.x * mouseX * targetMoveSpeed;
		cameraTarget_.y -= right.y * mouseX * targetMoveSpeed;
		cameraTarget_.z -= right.z * mouseX * targetMoveSpeed;
		cameraTarget_.x += up.x * mouseY * targetMoveSpeed;
		cameraTarget_.y += up.y * mouseY * targetMoveSpeed;
		cameraTarget_.z += up.z * mouseY * targetMoveSpeed;
	}
	if (isCameraDragging && (input->IsMouseButtonPressed(1) || input->IsMouseButtonPressed(2))) {
		cameraYaw_ += static_cast<float>(input->GetMouseX()) * rotateSpeed;
		cameraPitch_ += static_cast<float>(input->GetMouseY()) * rotateSpeed;
		cameraPitch_ = std::clamp(cameraPitch_, minPitch, maxPitch);
	}
	if (isMouseOverGameView && input->GetMouseWheel() != 0) {
		cameraDistance_ -= static_cast<float>(input->GetMouseWheel()) * zoomSpeed;
		cameraDistance_ = (std::max)(0.5f, cameraDistance_);
	}

	const float yawCos = std::cos(cameraYaw_);
	const float yawSin = std::sin(cameraYaw_);
	const float pitchCos = std::cos(cameraPitch_);
	const float pitchSin = std::sin(cameraPitch_);
	const Vector3 forward{ yawSin * pitchCos, -pitchSin, yawCos * pitchCos };
	camera->SetTranslate({
		cameraTarget_.x - forward.x * cameraDistance_,
		cameraTarget_.y - forward.y * cameraDistance_,
		cameraTarget_.z - forward.z * cameraDistance_,
	});
	camera->SetRotate({ cameraPitch_, cameraYaw_, 0.0f });
	return true;
}

// 外側をクリックした時に、プレビューを閉じてよい状態かを判定します。
bool DebugAssetPreview::ShouldExitOnOutsideClick(
	bool isLeftMouseDown,
	bool isLeftMouseClicked,
	bool isMouseOverGameView)
{
	if (!isActive_) {
		return false;
	}
	if (suppressExitUntilMouseRelease_) {
		if (!isLeftMouseDown) {
			suppressExitUntilMouseRelease_ = false;
		}
		return false;
	}
	return isLeftMouseClicked && !isMouseOverGameView;
}
