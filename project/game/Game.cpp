#include "Game.h"

#include"SceneFactory.h"
#include "GameSceneRegistration.h"
#include "SceneManager.h"
#include <algorithm>
#include <cstdlib>
#include <string>

namespace {
std::string GetEnvironmentString(const char* name)
{
	// _dupenv_sで確保された文字列は、この関数内で必ず解放する。
	char* value = nullptr;
	size_t size = 0;
	if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
		return {};
	}

	std::string result(value);
	std::free(value);
	return result;
}
}

// 共通資源の準備後に開始Sceneを予約し、生成に失敗した時は空画面の更新へ進めません。
void Game::Initialize()
{
	Framework::Initialize();
	// 共通描画の必須資源が欠けた時は、未完成の環境へSceneを登録しません。
	if (IsEndRequest()) {
		return;
	}

	// 共通資源の準備後にゲーム固有Sceneを登録し、静的初期化順に依存させません。
	auto gameSceneFactory = std::make_unique<SceneFactory>();
	RegisterGameScenes(*gameSceneFactory);
	SetSceneFactory(std::move(gameSceneFactory));

	// 環境変数を指定すると、通常のLoading画面を経由せず対象Sceneを起動できます。
	const std::string startSceneName = GetEnvironmentString("RIBONIN_CHALLENGE_START_SCENE");
	// 生成できないSceneを指定された時は、何も描画しない更新ループへ進めません。
	if (!Engine::SceneManager::GetInstance()->ChangeScene(startSceneName.empty() ? "LOADING" : startSceneName)) {
		RequestEnd(1);
		return;
	}
	InitializeSceneStressFromEnvironment();
	startupRouteSmoke_.Initialize(
		GetEnvironmentString("RIBONIN_CHALLENGE_STARTUP_ROUTE_SMOKE") == "1",
		startSceneName.empty());
}

// 共通更新で終了したフレームにはScene操作を足さず、テスト終了時の成否だけを返します。
void Game::Update()
{
	//基底クラスのUpdateを呼ぶだけで、マネージャー経由でシーンを更新
	Framework::Update();
	if (IsEndRequest()) {
		return;
	}
	UpdateSceneStress();
	startupRouteSmoke_.Update();
	if (startupRouteSmoke_.IsFinished()) {
		RequestEnd(startupRouteSmoke_.WasSuccessful() ? 0 : 1);
	}
}

// 現在Sceneの描画後に自動確認へ知らせ、描画回数を更新順と取り違えません。
void Game::Draw()
{
	Engine::SceneManager::GetInstance()->Draw();
	startupRouteSmoke_.AfterDraw();
}

void Game::InitializeSceneStressFromEnvironment()
{
	// 自動テスト時だけ有効にするため、設定がなければ何も変更しない。
	const std::string restartCountText = GetEnvironmentString("RIBONIN_CHALLENGE_SCENE_STRESS_RESTARTS");
	if (restartCountText.empty()) {
		return;
	}

	sceneStressRequestedRestarts_ = (std::max)(0, std::atoi(restartCountText.c_str()));
	if (sceneStressRequestedRestarts_ <= 0) {
		return;
	}

	const std::string targetSceneText = GetEnvironmentString("RIBONIN_CHALLENGE_SCENE_STRESS_SCENE");
	if (!targetSceneText.empty()) {
		sceneStressTarget_ = targetSceneText;
	}
	const std::string intervalText = GetEnvironmentString("RIBONIN_CHALLENGE_SCENE_STRESS_INTERVAL");
	if (!intervalText.empty()) {
		sceneStressIntervalFrames_ = (std::max)(1, std::atoi(intervalText.c_str()));
	}

	// 再起動回数が有効なら、UpdateSceneStressでフレームごとに進行する。
	sceneStressEnabled_ = true;
	sceneStressCompletedRestarts_ = 0;
	sceneStressFrameCounter_ = 0;
}

void Game::UpdateSceneStress()
{
	if (!sceneStressEnabled_) {
		return;
	}

	Engine::SceneManager* sceneManager = Engine::SceneManager::GetInstance();
	// シーン切り替え中は次の要求を積まず、完了を待つ。
	if (sceneManager->HasPendingScene()) {
		return;
	}

	// まず対象シーンへ移動してから再起動回数を数える。
	if (sceneManager->GetCurrentSceneName() != sceneStressTarget_) {
		sceneManager->ChangeScene(sceneStressTarget_);
		return;
	}

	++sceneStressFrameCounter_;
	if (sceneStressCompletedRestarts_ < sceneStressRequestedRestarts_ &&
		sceneStressFrameCounter_ >= sceneStressIntervalFrames_) {
		sceneStressFrameCounter_ = 0;
		if (sceneManager->RestartCurrentScene()) {
			++sceneStressCompletedRestarts_;
		}
		return;
	}

	// 指定回数を終えたらフレームワークの終了要求を出す。
	if (sceneStressCompletedRestarts_ >= sceneStressRequestedRestarts_ &&
		!sceneManager->HasPendingScene()) {
		RequestEnd();
	}
}
