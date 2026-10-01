#pragma once

#include <memory>
#include <string>

class Object3d;
class Object3dCommon;

// Object3dを生成する時の共通手順を一か所へ集めるFactoryです。
// Sceneは生成後に、自分のCamera・Light・位置などゲーム固有の設定だけを追加します。
class Object3dFactory
{
public:
	// モデルを読み込み、描画できるObject3dを返します。失敗時はnullptrです。
	static std::unique_ptr<Object3d> Create(
		Object3dCommon* object3dCommon,
		const std::string& modelName,
		bool initializeAnimation = false);
	// 既に所有しているObject3dへ、Factoryと同じモデル読込・初期化手順を適用します。
	// PlayerやMirrorのようにObject3dを値として持つクラスで使用します。
	static bool InitializeObject(
		Object3d& object,
		Object3dCommon* object3dCommon,
		const std::string& modelName,
		bool initializeAnimation = false);
	// Skeletalモデルの同名Animationをresourcesから読み、Loop再生を開始します。開始できた時だけtrueです。
	static bool LoadAndPlayAnimation(Object3d& object, const std::string& modelName);
};
