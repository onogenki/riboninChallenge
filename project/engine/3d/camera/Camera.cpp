#include "Camera.h"
#include "MyMath.h"
#include "DirectXCommon.h"

using namespace MyMath;

Camera::Camera()
	: transform({ 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f })
	, fovY(0.45f)
	, aspectRatio(float(DirectXCommon::GetInstance()->GetClientWidth()) / float(DirectXCommon::GetInstance()->GetClientHeight()))
	, nearClip(0.1f)
	, farClip(100.0f)
	, worldMatrix(MakeAffineMatrix(transform.scale, transform.rotate, transform.translate))
	, viewMatrix(Inverse(worldMatrix))
	, projectionMatrix(MakePerspectiveFovMatrix(fovY, aspectRatio, nearClip, farClip))
	, viewProjectionMatrix(Multiply(viewMatrix, projectionMatrix))
{}

void Camera::Update()
{
	const uint32_t clientWidth = DirectXCommon::GetInstance()->GetClientWidth();
	const uint32_t clientHeight = DirectXCommon::GetInstance()->GetClientHeight();
	if (clientHeight != 0) {
		aspectRatio = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);
	}
	//カメラのワールド行列
	worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	//ビュー行列行列
	viewMatrix = Inverse(worldMatrix);
	//プロジェクション行列
	projectionMatrix = MakePerspectiveFovMatrix(fovY, aspectRatio, nearClip, farClip);
	//合成行列
	viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);
}
