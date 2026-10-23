#include "GameOverScene.h"

#include "Camera.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "Logger.h"
#include "Object3dCommon.h"
#include "Object3dFactory.h"
#include "PostEffect.h"
#include "SceneManager.h"
#include "SceneRenderPipeline.h"
#include "SkyBox.h"
#include <dinput.h>
#include <Windows.h>

int GameOverScene::retryStageId_ = 1;

GameOverScene::GameOverScene() = default;
GameOverScene::~GameOverScene() = default;

// 不正なIDで存在しないStageを作らないよう、既存Stageの範囲だけを記録します。
void GameOverScene::SetRetryStageId(int stageId)
{
	if (stageId >= 1 && stageId <= 5) {
		retryStageId_ = stageId;
	}
}

// StageSelectと同じ背景用Camera・夜空を作り、素材が欠けた場合は描画へ進めません。
void GameOverScene::Initialize()
{
	isReady_ = false;
	isLeaving_ = false;
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);
	InitializeMainCamera(cameraManager_, mainCamera_, { 0.0f, 0.0f, 0.0f });
	Object3dCommon::GetInstance()->Initialize(DirectXCommon::GetInstance());
	Object3dCommon::GetInstance()->SetDefaultCamera(cameraManager_->GetActiveCamera());
	skyBox_ = Object3dFactory::CreateSkyBox(
		DirectXCommon::GetInstance(), cameraManager_->GetActiveCamera(),
		"Resources/backGround/skybox-night-cube.dds");
	if (!skyBox_) {
		Logger::Log("GameOver initialization failed: SkyBox DDS could not be prepared.");
		PostQuitMessage(1);
		return;
	}
	isReady_ = true;
}

// 死亡時のStage IDを保持したまま、BackspaceとEnterの片方だけでSceneを切り替えます。
void GameOverScene::Update()
{
	if (!isReady_) {
		return;
	}
	cameraManager_->Update();
	skyBox_->Update();
	ImGuiManager::GetInstance()->Begin("GameOver");
	ImGuiManager::GetInstance()->End();
	if (isLeaving_ || !ImGuiManager::GetInstance()->IsGameViewActive()) {
		return;
	}
	Input* input = Input::GetInstance();
	if (input->TriggerKey(DIK_BACK)) {
		isLeaving_ = SceneManager::GetInstance()->ChangeSceneWithFade("STAGE_SELECT");
	} else if (input->TriggerKey(DIK_RETURN)) {
		isLeaving_ = SceneManager::GetInstance()->ChangeSceneWithFade("STAGE_RETRY");
	}
}

// Title・StageSelectと同じ描画Pipelineで夜空を表示し、専用の画像は加えません。
void GameOverScene::Draw()
{
	if (!isReady_) {
		return;
	}
	SceneRenderPipeline::Begin(DirectXCommon::GetInstance());
	skyBox_->Draw();
	SceneRenderPipeline::End(DirectXCommon::GetInstance(), cameraManager_->GetActiveCamera());
}

// Scene終了時は共通資源を所有せず、GameOver固有のCameraとSkyBoxだけ解放します。
void GameOverScene::Finalize()
{
	isReady_ = false;
	skyBox_.reset();
	mainCamera_.reset();
	cameraManager_.reset();
}
