#include "Camera.h"
#include "MyMath.h"
#include "DirectXCommon.h"

using namespace MyMath;

namespace Engine
{
// メンバ宣言順にCamera設定を先に確定し、有効な値から最初の投影行列を作ります。
// エンジンが持つCameraの初期値は従来と同じにし、Sceneの見た目を変えません。
Camera::Camera()
	: transform_({ 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f })
	, fovY_(0.45f)
	, aspectRatio_(float(DirectXCommon::GetInstance()->GetClientWidth()) / float(DirectXCommon::GetInstance()->GetClientHeight()))
	, nearClip_(0.1f)
	, farClip_(100.0f)
	, worldMatrix_(MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate))
	, viewMatrix_(Inverse(worldMatrix_))
	, projectionMatrix_(MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_))
	, viewProjectionMatrix_(Multiply(viewMatrix_, projectionMatrix_))
{}

// Windowの縦横比を先に確定し、位置・向きからビュー行列、画角から投影行列を作り直します。
// 型の所属を移しても、毎フレームの行列更新順と設定値は維持します。
void Camera::Update()
{
	const uint32_t clientWidth = DirectXCommon::GetInstance()->GetClientWidth();
	const uint32_t clientHeight = DirectXCommon::GetInstance()->GetClientHeight();
	if (clientHeight != 0) {
		aspectRatio_ = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);
	}
	//カメラのワールド行列
	worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	//ビュー行列行列
	viewMatrix_ = Inverse(worldMatrix_);
	//プロジェクション行列
	projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	//合成行列
	viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
}
}
