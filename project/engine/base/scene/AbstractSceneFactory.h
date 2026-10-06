#pragma once

#include"BaseScene.h"
#include<string>
#include<vector>
#include<memory>

// Scene名から新しいSceneを作るための共通の入口です。
// SceneManagerはゲーム固有のScene型を知らず、このFactoryだけに生成を依頼します。
class AbstractSceneFactory
{
public:

	// 派生Factoryを基底ポインタから安全に破棄できるようにします。
	virtual ~AbstractSceneFactory() = default;
	// sceneNameのSceneを生成して所有権を渡します。見つからなければnullptrです。
	virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;
	// 登録されたScene名一覧を、Factoryが生きている間だけ参照で返します。
	virtual const std::vector<std::string>& GetSceneNames() const = 0;

};
