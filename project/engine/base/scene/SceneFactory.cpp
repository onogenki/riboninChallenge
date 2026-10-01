#include "SceneFactory.h"
#include "GameSceneRegistry.h"
#include "TitleScene.h"    
#include "DebugScene.h"
#include <utility>

// このゲームで切り替えられるScene名と生成関数を登録します。
SceneFactory::SceneFactory()
{
	// 新しいシーンは生成関数と名前をここへ登録する。
	RegisterScene("TITLE", []() { return std::make_unique<TitleScene>(); });
	// 現在の構成ではUSE_IMGUIがDebug/Developmentにだけあり、Releaseには登録しません。
#ifdef USE_IMGUI
	RegisterScene("DEBUG", []() { return std::make_unique<DebugScene>(); });
#endif
	GameSceneRegistry::Apply(*this);
}

// Scene名と生成関数を一組として登録し、重複名は表示一覧へ追加しません。
void SceneFactory::RegisterScene(const std::string& sceneName, SceneCreator creator)
{
	if (creators_.emplace(sceneName, std::move(creator)).second) {
		sceneNames_.push_back(sceneName);
	}
}

// Scene名を検索し、対応する生成関数があれば新しいSceneを返します。
std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
{
	const auto creator = creators_.find(sceneName);
	return creator != creators_.end() ? creator->second() : nullptr;
}
