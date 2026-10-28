#pragma once

#include "BaseScene.h"
#include <memory>

namespace Engine { class CameraManager; }
namespace Engine { class Camera; }
class GameOverOverlay;
namespace Engine { class SkyBox; }

// 共通の夜空を表示し、Backspaceで選択画面、Enterで失敗したStageを最初から作り直します。
class GameOverScene : public BaseScene
{
public:
	GameOverScene();
	~GameOverScene() override;
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
	bool IsReadyForTransition() const override { return isReady_; }

	// Scene切替中も死亡したStage IDを失わないよう、次のやり直し先だけ保持します。
	static void SetRetryStageId(int stageId);
	static int GetRetryStageId() { return retryStageId_; }

private:
	static int retryStageId_;
	std::unique_ptr<Engine::CameraManager> cameraManager_;
	std::unique_ptr<Engine::Camera> mainCamera_;
	std::unique_ptr<Engine::SkyBox> skyBox_;
	std::unique_ptr<GameOverOverlay> overlay_;
	bool isReady_ = false;
	bool isLeaving_ = false;
};
