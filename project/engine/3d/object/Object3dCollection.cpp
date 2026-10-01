#include "Object3dCollection.h"

#include "Object3dFactory.h"

// Sceneが使うObject3dCommonを保存し、Create時の初期化経路を固定します。
void Object3dCollection::Initialize(Object3dCommon* object3dCommon)
{
	object3dCommon_ = object3dCommon;
}

// Factoryで作ったObject3dをCollectionへ移し、Sceneへは非所有ポインタだけを返します。
Object3d* Object3dCollection::Create(const std::string& modelName)
{
	if (!object3dCommon_) {
		return nullptr;
	}

	std::unique_ptr<Object3d> object = Object3dFactory::Create(object3dCommon_, modelName);
	if (!object) {
		return nullptr;
	}
	Object3d* createdObject = object.get();
	objects_.push_back(std::move(object));
	return createdObject;
}

// Scene終了時に、Collectionが所有するObject3dをまとめて解放します。
void Object3dCollection::Clear()
{
	objects_.clear();
}
