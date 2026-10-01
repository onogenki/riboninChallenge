#pragma once

#include "Object3d.h"
#include <memory>
#include <string>
#include <vector>

class Object3dCommon;

// 一つのSceneが所有する通常3D Objectの生成・一覧・解放をまとめるコンテナです。
// Camera・Light・Drawの規則は持たず、Object3dの寿命だけを明確にします。
class Object3dCollection
{
public:
	// 以降のCreateで使うObject3dCommonを設定します。
	void Initialize(Object3dCommon* object3dCommon);
	// Factory経由でモデルを生成し、成功時はCollection所有のObject3dを返します。
	Object3d* Create(const std::string& modelName);
	// Scene終了時に、所有する全Object3dをまとめて解放します。
	void Clear();
	// RenderContextやScene固有のDrawへ、所有一覧を渡します。
	const std::vector<std::unique_ptr<Object3d>>& GetObjects() const { return objects_; }

private:
	Object3dCommon* object3dCommon_ = nullptr;
	std::vector<std::unique_ptr<Object3d>> objects_;
};
