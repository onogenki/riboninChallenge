#pragma once

#include <memory>
#include <string>

namespace Engine { class Object3d; }
namespace Engine { class Object3dCommon; }
namespace Engine { class Camera; }
namespace Engine { class DirectXCommon; }
namespace Engine { class SkyBox; }

namespace Engine
{
// Object3dを生成する時の共通手順を一か所へ集めるFactoryです。
// Sceneは生成後に、自分のCamera・Light・位置などゲーム固有の設定だけを追加します。
class Object3dFactory
{
public:
	// @brief 共有Modelと個体用GPU資源を準備し、完成したObjectだけを呼び出し元へ渡します。
	// @param object3dCommon 初期化済みの共通描画設定です。Factoryは所有しません。
	// @param modelName resources内で読み込むModelの名前です。空文字列は失敗します。
	// @param initializeAnimation trueならSkeletalモデル用のSkinClusterも準備します。
	// @return 所有権を持つObjectです。必須資源を準備できなければnullptrです。
	static std::unique_ptr<Engine::Object3d> Create(
		Engine::Object3dCommon* object3dCommon,
		const std::string& modelName,
		bool initializeAnimation = false);
	// @brief DDSを登録してからSkyBoxのGPU資源を作り、未登録Textureを描画へ渡しません。
	// @param directXCommon 初期化済みのDirectX共通処理です。Factoryは所有しません。
	// @param camera SkyBoxが描画時に借りるCameraです。Factoryは所有しません。
	// @param texturePath 読み込むDDSのパスです。空文字列は失敗します。
	// @return 所有権を持つSkyBoxです。読込や初期化に失敗すればnullptrです。
	static std::unique_ptr<Engine::SkyBox> CreateSkyBox(
		Engine::DirectXCommon* directXCommon,
		Engine::Camera* camera,
		const std::string& texturePath);
	// @brief PlayerやMirrorが所有する既存Objectへ、Createと同じ初期化手順を適用します。
	// @param object 呼び出し元が所有するObjectです。Factoryへ所有権を移しません。
	// @param object3dCommon 初期化済みの共通描画設定です。Factoryは所有しません。
	// @param modelName 読み込むModelの名前です。空文字列は失敗します。
	// @param initializeAnimation trueならSkeletalモデル用のSkinClusterも準備します。
	// @return 描画に必要な準備がすべて成功した時だけtrueです。
	static bool InitializeObject(
		Engine::Object3d& object,
		Engine::Object3dCommon* object3dCommon,
		const std::string& modelName,
		bool initializeAnimation = false);
	// @brief Skeletalモデルと同名のAnimationを読み、Loop再生を開始します。
	// @param object 呼び出し元が所有し、初期化済みのObjectです。
	// @param modelName resources内で探すAnimationの名前です。
	// @return 骨と有効な再生時間があり、Animationを開始できた時だけtrueです。
	static bool LoadAndPlayAnimation(Engine::Object3d& object, const std::string& modelName);
};
} // namespace Engine
