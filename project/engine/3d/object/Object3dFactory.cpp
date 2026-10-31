#include "Object3dFactory.h"

#include "ModelManager.h"
#include "Model.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "SkyBox.h"
#include "TextureManager.h"

namespace Engine
{
// モデルの読込とObject3d初期化を一箇所にまとめ、描画可能なObject3dを返します。
std::unique_ptr<Engine::Object3d> Object3dFactory::Create(
	Engine::Object3dCommon* object3dCommon,
	const std::string& modelName,
	bool initializeAnimation)
{
	auto object = std::make_unique<Engine::Object3d>();
	if (!InitializeObject(*object, object3dCommon, modelName, initializeAnimation)) {
		return nullptr;
	}
	return object;
}

// 必須DDSと四つのGPU BufferがそろったSkyBoxだけをSceneへ渡します。
std::unique_ptr<Engine::SkyBox> Object3dFactory::CreateSkyBox(
	Engine::DirectXCommon* directXCommon,
	Engine::Camera* camera,
	const std::string& texturePath)
{
	if (!directXCommon || !camera || texturePath.empty() ||
		!Engine::TextureManager::GetInstance()->LoadTexture(texturePath)) {
		return nullptr;
	}

	auto skyBox = std::make_unique<Engine::SkyBox>();
    if (!skyBox->Initialize(directXCommon, camera)) {
        return nullptr;
    }
	skyBox->SetTexture(texturePath);
	return skyBox;
}

// モデル・一体分の描画データ・必要なSkinClusterがそろったObjectだけを渡します。
bool Object3dFactory::InitializeObject(
	Engine::Object3d& object,
	Engine::Object3dCommon* object3dCommon,
	const std::string& modelName,
	bool initializeAnimation)
{
	// 描画共通設定やモデル名が無い状態では、半端なObject3dを残しません。
	if (!object3dCommon || modelName.empty() || !Engine::ModelManager::GetInstance()->LoadModel(modelName)) {
		return false;
	}

	if (!object.Initialize(object3dCommon)) {
		return false;
	}
	object.SetModel(modelName);
	if (initializeAnimation) {
		// SkeletalモデルだけがSkinClusterを作るため、通常モデルにも安全に呼べます。
		if (!object.InitializeAnimation()) {
			return false;
		}
	}
	return true;
}

// Skeletalモデルだけ、モデルと同名のAnimationを読み込んでLoop再生します。
bool Object3dFactory::LoadAndPlayAnimation(Engine::Object3d& object, const std::string& modelName)
{
	if (!object.IsSkeletal() || modelName.empty()) {
		return false;
	}

	const Engine::Model::Animation animation = Engine::Model::LoadAnimationFile("./resources", modelName);
	if (animation.duration <= 0.0f) {
		return false;
	}

	object.PlayAnimation(animation);
	object.SetIsLoop(true);
	return true;
}
} // namespace Engine
