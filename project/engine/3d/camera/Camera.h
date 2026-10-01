#pragma once
#include "Transform.h"
#include "Matrix4x4.h"

class Camera
{
public:
	Camera();

	void Update();

	//getter
	const Transform& GetTransform() const { return transform; }
	const Vector3& GetRotate() const { return transform.rotate; }
	const Vector3& GetTranslate()const { return transform.translate; }
	const Matrix4x4& GetWorldMatrix()const { return worldMatrix; }
	const Matrix4x4& GetViewMatrix() const { return viewMatrix; }
	const Matrix4x4& GetProjectionMatrix()const { return projectionMatrix; }
	const Matrix4x4& GetViewProjectionMatrix()const { return viewProjectionMatrix; }

	//setter
	void SetRotate(const Vector3& rotate) { transform.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform.translate = translate; }
	void SetFovY(float fovY) { this->fovY = fovY; }
	void SetAspectRatio(float aspectRatio) { this->aspectRatio = aspectRatio; }
	void SetNearClip(float nearClip) { this->nearClip = nearClip; }
	void SetFarClip(float farClip) { this->farClip = farClip; }

private:
	Transform transform;
	Matrix4x4 worldMatrix;
	Matrix4x4 viewMatrix;
	Matrix4x4 projectionMatrix;
	Matrix4x4 viewProjectionMatrix;
	
	float fovY;
	float aspectRatio;
	float nearClip;
	float farClip;

};

