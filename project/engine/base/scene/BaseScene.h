#pragma once
#include "Vector3.h"
#include <memory>

// 前方宣言により、全Sceneが不要な描画・Audioヘッダを読み込まないようにします。
class SceneManager;
class Object3dCommon;
class SpriteCommon;
class CameraManager;
class Camera;

class BaseScene
{
public:
	// 前方宣言したCamera型を安全に扱うため、実装はBaseScene.cppに置きます。
	BaseScene();
	// unique_ptrが前方宣言したCamera型を安全に破棄できるよう、実装はBaseScene.cppに置きます。
	virtual ~BaseScene();

	//純粋仮想関数
	virtual void Initialize() = 0;
	virtual void Finalize() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;
	virtual bool IsFinished() const { return false; }

protected:
	// 全Scene共通のCameraManagerとMainCameraを作り、指定位置・回転で最初の有効Cameraにします。
	void InitializeMainCamera(const Vector3& translate, const Vector3& rotate = {});

	Object3dCommon* object3dCommon = nullptr;
	SpriteCommon* spriteCommon = nullptr;

	std::unique_ptr<CameraManager> cameraManager;
	std::unique_ptr<Camera> mainCamera;

};

