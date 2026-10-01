#include "FixedMirror.h"

#include "Collision.h"
#include "DirectXCommon.h"
#include "MyMath.h"
#include "Object3dCommon.h"
#include "Object3dFactory.h"
#include "SrvManager.h"
#include <algorithm>
#include <cmath>

using namespace MyMath;

bool FixedMirror::Initialize(
	Object3dCommon* object3dCommon,
	DirectXCommon* dxCommon,
	SrvManager* srvManager,
	const std::string& modelName,
	const Vector3& center,
	float yaw,
	float width,
	float height,
	uint32_t reflectionTextureSize)
{
	if (!object3dCommon || !dxCommon || !srvManager) {
		return false;
	}

	const float safeWidth = (std::max)(width, 0.1f);
	const float safeHeight = (std::max)(height, 0.1f);
	yaw_ = yaw;
	mirror_ = Mirror(center, { std::sin(yaw_), 0.0f, std::cos(yaw_) }, safeWidth, safeHeight);
	// 大型Mirrorの裏面は反射せず、通常の板として扱います。
	mirror_.SetReflectBackface(false);

	// 値として所有するMirrorモデルも、Factoryの共通初期化経路を使います。
	if (!Object3dFactory::InitializeObject(object_, object3dCommon, modelName)) {
		return false;
	}
	// 反射Textureがまだ一枚も作られていない時や裏面を見た時にも、UVチェッカーを表示しません。
	object_.SetTextureOverride("resources/debug/white.png");
	// plane.objの板に厚みを持たせ、Playerが鏡をすり抜けないようにします。
	collider_.SetLocalShape({}, { 1.0f, 1.0f, 0.05f });
	SyncVisualAndCollider();

	const uint32_t safeTextureSize = (std::max)(reflectionTextureSize, 64u);
	return reflectionTarget_.Initialize(
		dxCommon,
		srvManager,
		safeTextureSize,
		safeTextureSize);
}

void FixedMirror::UpdateReflectionCamera(const Camera& sourceCamera, const Vector3& sourceForward)
{
	// 通常Cameraの位置と正面方向を鏡面で反転します。
	const Vector3 reflectionPosition = mirror_.ReflectPoint(sourceCamera.GetTranslate());
	const Vector3 reflectionForward = Normalize(mirror_.ReflectDirection(sourceForward));
	const float clampedY = std::clamp(reflectionForward.y, -1.0f, 1.0f);
	const float reflectionPitch = -std::asin(clampedY);
	const float reflectionYaw = std::atan2(reflectionForward.x, reflectionForward.z);

	reflectionCamera_.SetTranslate(reflectionPosition);
	reflectionCamera_.SetRotate({ reflectionPitch, reflectionYaw, 0.0f });
	reflectionCamera_.SetAspectRatio(
		static_cast<float>(reflectionTarget_.GetWidth()) /
		static_cast<float>(reflectionTarget_.GetHeight()));
	reflectionCamera_.Update();
}

void FixedMirror::SyncVisualAndCollider()
{
	// plane.objは-1から+1の板なので、幅と高さの半分をScaleへ設定します。
	object_.SetTranslate(mirror_.GetCenter());
	object_.SetScale({ mirror_.GetWidth() * 0.5f, mirror_.GetHeight() * 0.5f, 1.0f });
	object_.SetRotate({ pitch_, yaw_, 0.0f });
	// 3D回転後のローカル+Zを取り出し、Laserと反射Cameraで使う鏡面法線にします。
	const Matrix4x4 rotationMatrix = MakeAffineMatrix(
		{ 1.0f, 1.0f, 1.0f },
		{ pitch_, yaw_, 0.0f },
		{ 0.0f, 0.0f, 0.0f });
	mirror_.SetNormal({
		rotationMatrix.m[2][0],
		rotationMatrix.m[2][1],
		rotationMatrix.m[2][2],
	});
	collider_.SyncTransform(object_.GetTransform());
}

void FixedMirror::SetColliderShape(const Vector3& localCenter, const Vector3& localHalfSize)
{
	const Vector3 safeLocalHalfSize{
		(std::max)(std::abs(localHalfSize.x), 0.001f),
		(std::max)(std::abs(localHalfSize.y), 0.001f),
		(std::max)(std::abs(localHalfSize.z), 0.001f),
	};
	collider_.SetLocalShape(localCenter, safeLocalHalfSize);
	SyncVisualAndCollider();
}

void FixedMirror::BeginReflection(D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
{
	reflectionTarget_.Begin(depthStencilHandle);
}

void FixedMirror::EndReflection()
{
	reflectionTarget_.End();
	// Textureを作ったCamera行列も一緒に保存し、次の更新まで同じ組み合わせで表示します。
	capturedViewProjection_ = reflectionCamera_.GetViewProjectionMatrix();
	hasReflectionCapture_ = true;
}

void FixedMirror::DrawSurface()
{
	if (!reflectionTarget_.IsInitialized() || !hasReflectionCapture_) {
		object_.Draw();
		return;
	}
	object_.DrawMirror(
		reflectionTarget_.GetSrvIndex(),
		capturedViewProjection_);
}

void FixedMirror::DrawSurface(const Camera& camera)
{
	const Vector3 cameraFromMirror{
		camera.GetTranslate().x - mirror_.GetCenter().x,
		camera.GetTranslate().y - mirror_.GetCenter().y,
		camera.GetTranslate().z - mirror_.GetCenter().z,
	};
	// 裏面から見た大型Mirrorは、反射Textureではなく元の板モデルを描画します。
	if (Dot(cameraFromMirror, mirror_.GetNormal()) <= 0.0f) {
		object_.Draw();
		return;
	}
	if (!reflectionTarget_.IsInitialized() || !hasReflectionCapture_) {
		object_.Draw();
		return;
	}

	object_.DrawMirror(
		reflectionTarget_.GetSrvIndex(),
		capturedViewProjection_);
}
