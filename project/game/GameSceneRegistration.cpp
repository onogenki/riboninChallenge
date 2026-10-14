#include "GameSceneRegistration.h"

#include "GameSceneRegistry.h"
#include "loading/LoadingScene.h"
#include "SceneFactory.h"
#include "Stage1.h"
#include "stageSelect/StageSelectScene.h"
#include <memory>

namespace {
// 共通LoadingとStage1の生成規則をゲーム側へ置き、engine本体が鏡パズルへ依存しないようにします。
void RegisterGameScenes(SceneFactory& sceneFactory)
{
	// 起動時も本編開始時も、一つのLoadingSceneを共通して使います。
	sceneFactory.RegisterScene("LOADING", []() { return std::make_unique<LoadingScene>(); });
	// 提出用Titleは従来のStage1固定Cameraを使い、Enterで選択画面へ進みます。
	sceneFactory.RegisterScene("TITLE", []() { return std::make_unique<Stage1>(true); });
	sceneFactory.RegisterScene("STAGE_SELECT", []() { return std::make_unique<StageSelectScene>(); });
	sceneFactory.RegisterScene("STAGE1", []() { return std::make_unique<Stage1>(); });
}

// gameがリンクされた時だけ、起動前にStage1の登録処理をengineへ渡します。
const bool isStage1SceneRegistered = []()
{
	GameSceneRegistry::Register(RegisterGameScenes);
	return true;
}();
}
