#pragma once

#include "Camera.h"
#include "Collider.h"
#include "Mirror.h"
#include "MyMath.h"
#include "Object3d.h"
#include "PlanarReflectionTarget.h"
#include <string>

class DirectXCommon;
class Object3dCommon;
class SrvManager;

// 部屋やPlayerを映す、大型Mirrorです。
// 一枚ごとに専用の反射Cameraと描画先Textureを持ちます。
class FixedMirror
{
public:
	bool Initialize(
		Object3dCommon* object3dCommon,
		DirectXCommon* dxCommon,
		SrvManager* srvManager,
		const std::string& modelName,
		const Vector3& center,
		float yaw,
		float width,
		float height,
		uint32_t reflectionTextureSize = 512);

	// 通常Cameraを鏡面の反対側へ移し、反射Cameraを更新します。
	void UpdateReflectionCamera(const Engine::Camera& sourceCamera, const Vector3& sourceForward);
	// Mirrorの中心・大きさ・角度を、見た目と当たり判定へ反映します。
	void SyncVisualAndCollider();
	// モデルとColliderの中心や厚みが異なる場合に、ローカル座標の形を設定します。
	void SetColliderShape(const Vector3& localCenter, const Vector3& localHalfSize);
	// 専用Textureへの反射Scene描画を開始・終了します。
	void BeginReflection(D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle);
	void EndReflection();
	// 専用Textureを板へ貼り、鏡面を描画します。
	void DrawSurface();
	// Cameraが鏡の表側にいる時だけ、反射Textureを表示します。
	void DrawSurface(const Engine::Camera& camera);

	Object3d& GetObject() { return object_; }
	const Object3d& GetObject() const { return object_; }
	const Mirror& GetMirror() const { return mirror_; }
	Engine::Camera& GetReflectionCamera() { return reflectionCamera_; }
	const Engine::Camera& GetReflectionCamera() const { return reflectionCamera_; }
	// Playerなどが鏡のColliderへ判定を依頼する時に使います。
	const ObbCollider& GetCollider() const { return collider_; }
	// 床・壁一覧やLaser遮蔽へ渡す、鏡のOBB形状データを返します。
	const MyMath::OBB& GetObb() const { return collider_.GetShape(); }
	float GetYaw() const { return yaw_; }
	float GetPitch() const { return pitch_; }
	// 位置・角度・大きさを一度に反映し、反射面とColliderも同じ形へ同期します。
	void SetSurface(const Vector3& center, float yaw, float width, float height);
	// 回転ギミック用に、二軸の角度と反射面・Colliderを一度に同期します。
	void SetOrientation(float pitch, float yaw);
	// 鏡床だけは裏面からのLaserも反射するため、反射規則を切り替えます。
	void SetReflectBackface(bool enabled) { mirror_.SetReflectBackface(enabled); }
	bool IsReady() const { return reflectionTarget_.IsInitialized(); }
	bool HasReflectionCapture() const { return hasReflectionCapture_; }

private:
	Object3d object_;
	Mirror mirror_;
	Engine::Camera reflectionCamera_;
	PlanarReflectionTarget reflectionTarget_;
	// 見た目の板と同じTransformを受け取る、鏡専用のOBB Colliderです。
	ObbCollider collider_{};
	float pitch_ = 0.0f;
	float yaw_ = 0.0f;
	Matrix4x4 capturedViewProjection_{};
	bool hasReflectionCapture_ = false;
};
