#pragma once

#include "Object3d.h"
#include <array>
#include <memory>
#include <vector>

namespace Engine { class Camera; }

// Sceneが所有するCamera・Lightを、一つのObject3dへ渡すための値です。
// Object3d自身は「どのSceneのLightか」を知らず、このContextを受け取って描画準備だけを行います。
class Object3dRenderContext
{
public:
	// SpotLight一灯を使うScene向けの設定です。
	Object3dRenderContext(
		Engine::Camera* camera,
		const Engine::Object3d::DirectionalLight& directionalLight,
		const Engine::Object3d::PointLight& pointLight,
		const Engine::Object3d::SpotLight& spotLight);
	// 複数SpotLightを使うStage向けの設定です。
	Object3dRenderContext(
		Engine::Camera* camera,
		const Engine::Object3d::DirectionalLight& directionalLight,
		const Engine::Object3d::PointLight& pointLight,
		const std::array<Engine::Object3d::SpotLight, Engine::Object3d::kMaximumSpotLightCount>& spotLights);

	// DirectionalLightの方向を単位ベクトルへそろえ、長さ0なら安全な下向きを設定します。
	static void NormalizeDirectionalLight(Engine::Object3d::DirectionalLight& directionalLight);
	// Camera・Light・行列をObject3dへ反映してから更新します。
	void UpdateObject(Engine::Object3d& object) const;
	// Sceneが所有するObject3d一覧へ、同じCamera・Light・行列更新をまとめて適用します。
	void UpdateObjects(const std::vector<std::unique_ptr<Engine::Object3d>>& objects) const;

private:
	Engine::Camera* camera_ = nullptr;
	const Engine::Object3d::DirectionalLight* directionalLight_ = nullptr;
	const Engine::Object3d::PointLight* pointLight_ = nullptr;
	const Engine::Object3d::SpotLight* spotLight_ = nullptr;
	const std::array<Engine::Object3d::SpotLight, Engine::Object3d::kMaximumSpotLightCount>* spotLights_ = nullptr;
};
