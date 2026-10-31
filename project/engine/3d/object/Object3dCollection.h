#pragma once

#include "Object3d.h"
#include <memory>
#include <string>
#include <vector>

namespace Engine { class Object3dCommon; }

// 一つのSceneが所有する通常3D Objectの生成・一覧・解放をまとめるコンテナです。
// Camera・Light・Drawの規則は持たず、Object3dの寿命だけを明確にします。
class Object3dCollection
{
public:
	// @brief 以降のCreateで借りる共通描画設定を保存します。
	// @param object3dCommon Objectより長く生存する共通描画設定です。Collectionは所有しません。
	void Initialize(Engine::Object3dCommon* object3dCommon);
	// @brief Factoryで完成させたObjectをCollectionへ格納します。
	// @param modelName 読み込むModelの名前です。
	// @return Collectionが所有するObjectへの非所有ポインタです。失敗時はnullptrです。
	Engine::Object3d* Create(const std::string& modelName);
	// Scene終了時に全Objectを解放し、以前返した非所有ポインタを無効にします。
	void Clear();
	// @brief RenderContextやScene固有のDrawへ、Collection所有の一覧を貸します。
	// @return Collectionが生存する間だけ有効な所有一覧への参照です。
	const std::vector<std::unique_ptr<Engine::Object3d>>& GetObjects() const { return objects_; }

private:
	Engine::Object3dCommon* object3dCommon_ = nullptr;
	std::vector<std::unique_ptr<Engine::Object3d>> objects_;
};
// 従来の前方宣言を維持しながら、Engine名でも同じ型を参照できます。
namespace Engine { using Object3dCollection = ::Object3dCollection; }
