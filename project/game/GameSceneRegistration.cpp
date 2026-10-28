#include "GameSceneRegistration.h"

#include "gameOver/GameOverScene.h"
#include "loading/LoadingScene.h"
#include "SceneFactory.h"
#include "Stage.h"
#include "stageSelect/StageSelectScene.h"
#include <memory>

// ゲーム固有Sceneを起動時に登録し、静的初期化順への依存をなくします。
void RegisterGameScenes(SceneFactory& sceneFactory)
{
	// 起動時も本編開始時も、一つのLoadingSceneを共通して使います。
	sceneFactory.RegisterScene("LOADING", []() { return std::make_unique<LoadingScene>(); });
	// 提出用Titleは従来のStage1固定Cameraを使い、Enterで選択画面へ進みます。
	sceneFactory.RegisterScene("TITLE", []() { return std::make_unique<Stage>(1, true); });
	sceneFactory.RegisterScene("STAGE_SELECT", []() { return std::make_unique<StageSelectScene>(); });
	// Game Overは共通の夜空だけを描き、再挑戦時は直前のStage IDを使います。
	sceneFactory.RegisterScene("GAME_OVER", []() { return std::make_unique<GameOverScene>(); });
	sceneFactory.RegisterScene("STAGE_RETRY", []() { return std::make_unique<Stage>(GameOverScene::GetRetryStageId()); });
	// 本編は一つのStage Sceneを使い、Stage IDごとにstage1.jsonからstage5.jsonを読み分けます。
	sceneFactory.RegisterScene("STAGE1", []() { return std::make_unique<Stage>(1); });
}
