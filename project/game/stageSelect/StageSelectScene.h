#pragma once

#include "BaseScene.h"
#include <memory>

class CameraManager;
class Camera;
class SkyBox;
class StageSelectOverlay;

// 夜空と操作案内を表示し、EnterでStage1・BackspaceでTitleへ進む選択画面です。
class StageSelectScene : public BaseScene
{
public:
	StageSelectScene();
	~StageSelectScene() override;
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
	bool IsReadyForTransition() const override { return isReady_; }

private:
	std::unique_ptr<CameraManager> cameraManager_;
	std::unique_ptr<Camera> mainCamera_;
	std::unique_ptr<SkyBox> skyBox_;
	std::unique_ptr<StageSelectOverlay> overlay_;
	bool isReady_ = false;
};
