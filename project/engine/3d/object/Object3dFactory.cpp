#include "Object3dFactory.h"

#include "ModelManager.h"
#include "Model.h"
#include "Object3d.h"
#include "Object3dCommon.h"

// モデルの読込とObject3d初期化を一箇所にまとめ、描画可能なObject3dを返します。
std::unique_ptr<Object3d> Object3dFactory::Create(
	Object3dCommon* object3dCommon,
	const std::string& modelName,
	bool initializeAnimation)
{
	auto object = std::make_unique<Object3d>();
	if (!InitializeObject(*object, object3dCommon, modelName, initializeAnimation)) {
		return nullptr;
	}
	return object;
}

// 既に所有しているObject3dへ、Factoryと同じモデル読込・初期化手順を適用します。
bool Object3dFactory::InitializeObject(
	Object3d& object,
	Object3dCommon* object3dCommon,
	const std::string& modelName,
	bool initializeAnimation)
{
	// 描画共通設定やモデル名が無い状態では、半端なObject3dを残しません。
	if (!object3dCommon || modelName.empty() || !ModelManager::GetInstance()->LoadModel(modelName)) {
		return false;
	}

	object.Initialize(object3dCommon);
	object.SetModel(modelName);
	if (initializeAnimation) {
		// SkeletalモデルだけがSkinClusterを作るため、通常モデルにも安全に呼べます。
		object.InitializeAnimation();
	}
	return true;
}

// Skeletalモデルだけ、モデルと同名のAnimationを読み込んでLoop再生します。
bool Object3dFactory::LoadAndPlayAnimation(Object3d& object, const std::string& modelName)
{
	if (!object.IsSkeletal() || modelName.empty()) {
		return false;
	}

	const Model::Animation animation = Model::LoadAnimationFile("./resources", modelName);
	if (animation.duration <= 0.0f) {
		return false;
	}

	object.PlayAnimation(animation);
	object.SetIsLoop(true);
	return true;
}
