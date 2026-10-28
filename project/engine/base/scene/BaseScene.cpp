#include "BaseScene.h"

#include "Camera.h"
#include "CameraManager.h"

// CameraManagerとCameraを完全な型として読み込んだ場所で、基底Sceneを生成します。
BaseScene::BaseScene() = default;

// 派生Sceneが所有するCameraは、派生Scene自身の破棄時に解放します。
BaseScene::~BaseScene() = default;

// Scene自身の所有先へMainCameraを生成し、Managerへ登録して最初の有効Cameraにします。
void BaseScene::InitializeMainCamera(
	std::unique_ptr<Engine::CameraManager>& cameraManager,
	std::unique_ptr<Engine::Camera>& mainCamera,
	const Vector3& translate,
	const Vector3& rotate)
{
	cameraManager = std::make_unique<Engine::CameraManager>();
	mainCamera = std::make_unique<Engine::Camera>();
	mainCamera->SetRotate(rotate);
	mainCamera->SetTranslate(translate);
	cameraManager->AddCamera("MainCamera", mainCamera.get());
	cameraManager->SetActiveCamera("MainCamera");
}
