#include "StageClearScene.h"
#include "StageClearOverlay.h"

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
#include "SpriteCommon.h"
#include <dinput.h>
#include <Windows.h>

StageClearScene::StageClearScene() = default;
StageClearScene::~StageClearScene() = default;

// 既存画面と同じ夜空を作り、二枚の案内がそろってから描画を始めます。
void StageClearScene::Initialize()
{
	isReady_ = false;
	isLeaving_ = false;
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);
	InitializeMainCamera(cameraManager_, mainCamera_, { 0.0f, 0.0f, 0.0f });
	Engine::Object3dCommon::GetInstance()->Initialize(Engine::DirectXCommon::GetInstance());
	Engine::Object3dCommon::GetInstance()->SetDefaultCamera(cameraManager_->GetActiveCamera());
	skyBox_ = Engine::Object3dFactory::CreateSkyBox(
		Engine::DirectXCommon::GetInstance(), cameraManager_->GetActiveCamera(),
		"Resources/backGround/skybox-night-cube.dds");
	if (!skyBox_) {
		Logger::Log("StageClear initialization failed: SkyBox DDS could not be prepared.");
		PostQuitMessage(1);
		return;
	}
	overlay_ = std::make_unique<StageClearOverlay>();
	if (!overlay_->Initialize(Engine::SpriteCommon::GetInstance(), Engine::DirectXCommon::GetInstance())) {
		Logger::Log("StageClear initialization failed: navigation Sprite could not be prepared.");
		PostQuitMessage(1);
		return;
	}
	isReady_ = true;
}

// 背景と案内を更新し、Game ViewのBackspaceだけを一度のScene切替に使います。
void StageClearScene::Update()
{
	if (!isReady_) {
		return;
	}
	cameraManager_->Update();
	skyBox_->Update();
	overlay_->Update(
		static_cast<float>(Engine::DirectXCommon::GetInstance()->GetClientWidth()),
		static_cast<float>(Engine::DirectXCommon::GetInstance()->GetClientHeight()));
	ImGuiManager::GetInstance()->Begin("StageClear");
	ImGuiManager::GetInstance()->End();
	if (!isLeaving_ && ImGuiManager::GetInstance()->IsGameViewActive() &&
		Engine::Input::GetInstance()->TriggerKey(DIK_BACK)) {
		isLeaving_ = Engine::SceneManager::GetInstance()->ChangeSceneWithFade("STAGE_SELECT");
	}
}

// 夜空、クリア見出し、Backspace案内を共通Pipelineの順番で描画します。
void StageClearScene::Draw()
{
	if (!isReady_) {
		return;
	}
	SceneRenderPipeline::Begin(Engine::DirectXCommon::GetInstance());
	skyBox_->Draw();
	SceneRenderPipeline::DrawSprites(Engine::SpriteCommon::GetInstance(), overlay_->GetSprites());
	SceneRenderPipeline::End(Engine::DirectXCommon::GetInstance(), cameraManager_->GetActiveCamera());
}

// Scene終了時に専用Sprite・SkyBox・Cameraだけを解放します。
void StageClearScene::Finalize()
{
	isReady_ = false;
	if (overlay_) {
		overlay_->Finalize();
		overlay_.reset();
	}
	skyBox_.reset();
	mainCamera_.reset();
	cameraManager_.reset();
}
