#pragma once

#include "Object3d.h"
#include <array>
#include <memory>
#include <vector>

class Camera;

// Sceneが所有するCamera・Lightを、一つのObject3dへ渡すための値です。
// Object3d自身は「どのSceneのLightか」を知らず、このContextを受け取って描画準備だけを行います。
class Object3dRenderContext
{
public:
	// SpotLight一灯を使うScene向けの設定です。
	Object3dRenderContext(
		Camera* camera,
		const Object3d::DirectionalLight& directionalLight,
		const Object3d::PointLight& pointLight,
		const Object3d::SpotLight& spotLight);
	// 複数SpotLightを使うStage向けの設定です。
	Object3dRenderContext(
		Camera* camera,
		const Object3d::DirectionalLight& directionalLight,
		const Object3d::PointLight& pointLight,
		const std::array<Object3d::SpotLight, Object3d::kMaximumSpotLightCount>& spotLights);

	// DirectionalLightの方向を単位ベクトルへそろえ、長さ0なら安全な下向きを設定します。
	static void NormalizeDirectionalLight(Object3d::DirectionalLight& directionalLight);
	// Camera・Light・行列をObject3dへ反映してから更新します。
	void UpdateObject(Object3d& object) const;
	// Sceneが所有するObject3d一覧へ、同じCamera・Light・行列更新をまとめて適用します。
	void UpdateObjects(const std::vector<std::unique_ptr<Object3d>>& objects) const;

private:
	Camera* camera_ = nullptr;
	const Object3d::DirectionalLight* directionalLight_ = nullptr;
	const Object3d::PointLight* pointLight_ = nullptr;
	const Object3d::SpotLight* spotLight_ = nullptr;
	const std::array<Object3d::SpotLight, Object3d::kMaximumSpotLightCount>* spotLights_ = nullptr;
};
