#include "SceneFactory.h"
#include "Logger.h"
#include "game/title/TitleScene.h"
#include "DebugScene.h"
#include <utility>

// 従来のTitle・Debug登録を生成時に行い、Scene一覧の先頭と既存APIを保ちます。
SceneFactory::SceneFactory()
{
	RegisterScene("TITLE_ORIGINAL", []() { return std::make_unique<TitleScene>(); });
#ifdef USE_IMGUI
	RegisterScene("DEBUG", []() { return std::make_unique<DebugScene>(); });
#endif
}

// 空の名前・生成関数は登録せず、Scene切替時に呼べない関数が残るのを防ぎます。
// Scene名と生成関数を一組として登録し、重複名は表示一覧へ追加しません。
void SceneFactory::RegisterScene(const std::string& sceneName, SceneCreator creator)
{
	if (sceneName.empty() || !creator) {
		Logger::Log("Scene registration failed: scene name or creator is empty.");
		return;
	}
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
