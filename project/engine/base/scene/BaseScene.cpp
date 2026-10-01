#include "BaseScene.h"

#include "Camera.h"
#include "CameraManager.h"

// CameraManagerとCameraを完全な型として読み込んだ場所で、共通Sceneの所有物を生成します。
BaseScene::BaseScene() = default;

// CameraManagerとCameraを完全な型として読み込んだ場所で、共通Sceneの所有物を解放します。
BaseScene::~BaseScene() = default;

// Scene共通のMainCameraを生成し、CameraManagerへ登録して最初の有効Cameraにします。
void BaseScene::InitializeMainCamera(const Vector3& translate, const Vector3& rotate)
{
	cameraManager = std::make_unique<CameraManager>();
	mainCamera = std::make_unique<Camera>();
	mainCamera->SetRotate(rotate);
	mainCamera->SetTranslate(translate);
	cameraManager->AddCamera("MainCamera", mainCamera.get());
	cameraManager->SetActiveCamera("MainCamera");
}
