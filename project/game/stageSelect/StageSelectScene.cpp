#include "StageSelectScene.h"
#include "StageSelectOverlay.h"

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

StageSelectScene::StageSelectScene() = default;
StageSelectScene::~StageSelectScene() = default;

// Title・Stage1と同じ夜空に三枚の画像を重ね、入力前に全素材をそろえます。
void StageSelectScene::Initialize()
{
	isReady_ = false;
	PostEffect::GetInstance()->SetGrayscale(false);
	PostEffect::GetInstance()->SetSepia(false);
	InitializeMainCamera(cameraManager_, mainCamera_, { 0.0f, 0.0f, 0.0f });
	Engine::Object3dCommon::GetInstance()->Initialize(Engine::DirectXCommon::GetInstance());
	Engine::Object3dCommon::GetInstance()->SetDefaultCamera(cameraManager_->GetActiveCamera());
	skyBox_ = Engine::Object3dFactory::CreateSkyBox(
		Engine::DirectXCommon::GetInstance(), cameraManager_->GetActiveCamera(),
		"Resources/backGround/skybox-night-cube.dds");
	if (!skyBox_) {
		Logger::Log("StageSelect initialization failed: SkyBox DDS could not be prepared.");
		PostQuitMessage(1);
		return;
	}
	overlay_ = std::make_unique<StageSelectOverlay>();
	if (!overlay_->Initialize(Engine::SpriteCommon::GetInstance(), Engine::DirectXCommon::GetInstance())) {
		Logger::Log("StageSelect initialization failed: navigation Sprite could not be prepared.");
		PostQuitMessage(1);
		return;
	}
	isReady_ = true;
}

// 夜空と案内を更新し、BackspaceはTitle、Enterは本編へ一度だけ遷移させます。
void StageSelectScene::Update()
{
	if (!isReady_) {
		return;
	}
	cameraManager_->Update();
	skyBox_->Update();
	overlay_->Update(
		static_cast<float>(Engine::DirectXCommon::GetInstance()->GetClientWidth()),
		static_cast<float>(Engine::DirectXCommon::GetInstance()->GetClientHeight()));
	ImGuiManager::GetInstance()->Begin("StageSelect");
	ImGuiManager::GetInstance()->End();
	if (ImGuiManager::GetInstance()->IsGameViewActive()) {
		Engine::Input* input = Engine::Input::GetInstance();
		if (input->TriggerKey(DIK_BACK)) {
			Engine::SceneManager::GetInstance()->ChangeSceneWithFade("TITLE");
		} else if (input->TriggerKey(DIK_RETURN)) {
			Engine::SceneManager::GetInstance()->ChangeSceneWithFade("STAGE1");
		}
	}
}

// 3D夜空の後に操作案内を描き、白フェードと画面提示は共通Pipelineへ任せます。
void StageSelectScene::Draw()
{
	if (!isReady_) {
		return;
	}
	SceneRenderPipeline::Begin(Engine::DirectXCommon::GetInstance());
	skyBox_->Draw();
	SceneRenderPipeline::DrawSprites(Engine::SpriteCommon::GetInstance(), overlay_->GetSprites());
	SceneRenderPipeline::End(Engine::DirectXCommon::GetInstance(), cameraManager_->GetActiveCamera());
}

// GPU待機済みのScene終了時に、案内・背景・Cameraを所有順で解放します。
void StageSelectScene::Finalize()
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
