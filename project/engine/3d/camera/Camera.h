#pragma once
#include "Transform.h"
#include "Matrix4x4.h"

// 位置と向きから、描画に必要なビュー行列と透視投影行列を更新します。
class Camera
{
public:
	Camera();

	// 位置・向きと画角から描画用行列を更新します。Setterの後、描画前に呼びます。
	void Update();

	// 描画側が更新済みのCamera値を読むため、内部状態を変更できない参照で返します。
	const Transform& GetTransform() const { return transform_; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	const Vector3& GetTranslate()const { return transform_.translate; }
	const Matrix4x4& GetWorldMatrix()const { return worldMatrix_; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
	const Matrix4x4& GetProjectionMatrix()const { return projectionMatrix_; }
	const Matrix4x4& GetViewProjectionMatrix()const { return viewProjectionMatrix_; }

	// SceneやControllerの設定値を保存し、描画用行列はUpdate()でまとめて更新します。
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetFovY(float fovY) { fovY_ = fovY; }
	// Update()は画面サイズから縦横比を再計算するため、手動設定値はそこで上書きされます。
	void SetAspectRatio(float aspectRatio) { aspectRatio_ = aspectRatio; }
	void SetNearClip(float nearClip) { nearClip_ = nearClip; }
	void SetFarClip(float farClip) { farClip_ = farClip; }

private:
	Transform transform_;
	// 投影行列はこれらの設定値を使うため、C++のメンバ初期化順でも先に置きます。
	float fovY_;
	float aspectRatio_;
	float nearClip_;
	float farClip_;

	Matrix4x4 worldMatrix_;
	Matrix4x4 viewMatrix_;
	Matrix4x4 projectionMatrix_;
	Matrix4x4 viewProjectionMatrix_;

};

