#include "Object3dRenderContext.h"

#include "Camera.h"
#include "MyMath.h"

// DirectionalLightの方向はShaderへ単位ベクトルとして渡し、長さ0によるLight計算の破綻を防ぎます。
void Object3dRenderContext::NormalizeDirectionalLight(Object3d::DirectionalLight& directionalLight)
{
	if (MyMath::Length(directionalLight.direction) > 0.0f) {
		directionalLight.direction = MyMath::Normalize(directionalLight.direction);
	} else {
		directionalLight.direction = { 0.0f, -1.0f, 0.0f };
	}
}

// SpotLight一灯を使うScene用に、CameraとLightへの参照をまとめます。
Object3dRenderContext::Object3dRenderContext(
	Camera* camera,
	const Object3d::DirectionalLight& directionalLight,
	const Object3d::PointLight& pointLight,
	const Object3d::SpotLight& spotLight)
	: camera_(camera)
	, directionalLight_(&directionalLight)
	, pointLight_(&pointLight)
	, spotLight_(&spotLight)
{
}

// 複数SpotLightを使うStage用に、CameraとLightへの参照をまとめます。
Object3dRenderContext::Object3dRenderContext(
	Camera* camera,
	const Object3d::DirectionalLight& directionalLight,
	const Object3d::PointLight& pointLight,
	const std::array<Object3d::SpotLight, Object3d::kMaximumSpotLightCount>& spotLights)
	: camera_(camera)
	, directionalLight_(&directionalLight)
	, pointLight_(&pointLight)
	, spotLights_(&spotLights)
{
}

// Object3dへ現在のCamera・Lightを渡してから、行列などの描画準備を更新します。
void Object3dRenderContext::UpdateObject(Object3d& object) const
{
	object.SetCamera(camera_);
	if (directionalLight_) {
		object.SetDirectionalLight(*directionalLight_);
	}
	if (pointLight_) {
		object.SetPointLight(*pointLight_);
	}
	if (spotLights_) {
		object.SetSpotLights(*spotLights_);
	} else if (spotLight_) {
		object.SetSpotLight(*spotLight_);
	}
	object.Update();
}

// 空の要素を安全に飛ばしながら、Scene内の全Object3dを同じ描画条件で更新します。
void Object3dRenderContext::UpdateObjects(
	const std::vector<std::unique_ptr<Object3d>>& objects) const
{
	for (const std::unique_ptr<Object3d>& object : objects) {
		if (object) {
			UpdateObject(*object);
		}
	}
}
