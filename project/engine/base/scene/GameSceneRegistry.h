#pragma once

#include <functional>

class SceneFactory;

// game側が追加したScene登録処理を、engineが名前を知らずに受け取る一覧です。
class GameSceneRegistry
{
public:
	using Registrar = std::function<void(SceneFactory&)>;

	// game側の静的初期化から、Scene追加処理を一度だけ登録します。
	static void Register(Registrar registrar);
	// SceneFactory生成時に、登録済みのゲーム固有Sceneを追加します。
	static void Apply(SceneFactory& sceneFactory);
};
