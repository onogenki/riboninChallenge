#pragma once
#include "Vector3.h"
#include <memory>

// 前方宣言により、全Sceneが不要な描画・Audioヘッダを読み込まないようにします。
class SceneManager;
class Object3dCommon;
class SpriteCommon;
class CameraManager;
class Camera;

// SceneManagerが扱うScene共通の開始・更新・描画・終了の呼び出し口です。
// Cameraの所有は各Sceneに置き、共通の初期化だけをここで補助します。
class BaseScene
{
public:
	// 前方宣言したCamera型を安全に扱うため、実装はBaseScene.cppに置きます。
	BaseScene();
	// unique_ptrが前方宣言したCamera型を安全に破棄できるよう、実装はBaseScene.cppに置きます。
	virtual ~BaseScene();

	// SceneManagerが切替を確定した時に呼び、Scene固有の資源を準備します。
	virtual void Initialize() = 0;
	// SceneManagerがSceneを離れる時に呼び、Scene固有の資源を解放します。
	virtual void Finalize() = 0;
	// 毎フレーム、入力とScene内の状態を描画より先に更新します。
	virtual void Update() = 0;
	// Update済みの状態を描画し、描画先を閉じます。
	virtual void Draw() = 0;
	// 既存の状態照会APIです。現行のSceneManagerは参照せず、切替はChangeSceneで行います。
	virtual bool IsFinished() const { return false; }

protected:
	// 全Scene共通のCameraManagerとMainCameraを作り、指定位置・回転で最初の有効Cameraにします。
	void InitializeMainCamera(const Vector3& translate, const Vector3& rotate = {});

	// 描画共通設定は借り、Scene終了時にManager本体を解放しません。
	Object3dCommon* object3dCommon = nullptr;
	SpriteCommon* spriteCommon = nullptr;

	// Scene専用のCameraManagerとMainCameraはこのSceneが所有します。
	std::unique_ptr<CameraManager> cameraManager;
	std::unique_ptr<Camera> mainCamera;

};

