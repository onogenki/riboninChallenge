#pragma once

#include "BaseScene.h"
#include <memory>

namespace Engine { class CameraManager; }
namespace Engine { class Camera; }
namespace Engine { class SkyBox; }
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
	std::unique_ptr<Engine::CameraManager> cameraManager_;
	std::unique_ptr<Engine::Camera> mainCamera_;
	std::unique_ptr<Engine::SkyBox> skyBox_;
	std::unique_ptr<StageSelectOverlay> overlay_;
	bool isReady_ = false;
};
