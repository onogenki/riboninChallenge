#pragma once

#include "BaseScene.h"
#include <memory>

namespace Engine { class CameraManager; }
namespace Engine { class Camera; }
namespace Engine { class SkyBox; }
class StageClearOverlay;

// 共通の夜空にクリア案内を表示し、BackspaceでStageSelectへ戻します。
class StageClearScene : public BaseScene
{
public:
	StageClearScene();
	~StageClearScene() override;
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
	bool IsReadyForTransition() const override { return isReady_; }

private:
	std::unique_ptr<Engine::CameraManager> cameraManager_;
	std::unique_ptr<Engine::Camera> mainCamera_;
	std::unique_ptr<Engine::SkyBox> skyBox_;
	std::unique_ptr<StageClearOverlay> overlay_;
	bool isReady_ = false;
	bool isLeaving_ = false;
};
