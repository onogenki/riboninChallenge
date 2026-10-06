#pragma once

#include"AbstractSceneFactory.h"
#include <functional>
#include <unordered_map>
#include <vector>

// このゲームで使うScene名と生成関数を登録し、要求されたSceneを一体作ります。
// Sceneの寿命は受け取ったSceneManagerが管理します。
class SceneFactory : public AbstractSceneFactory
{
public:
	using SceneCreator = std::function<std::unique_ptr<BaseScene>()>;

	// このゲームで切り替えられるScene名と生成関数を登録します。
	SceneFactory();

	// 指定名のSceneを一体生成します。未登録名ならnullptrです。
	std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName)override;
	// 登録済みScene名を、Debug UIなどの一覧表示用に返します。
	const std::vector<std::string>& GetSceneNames() const override { return sceneNames_; }
	// game側が独自Sceneを追加するための、共通の登録窓口です。
	void RegisterScene(const std::string& sceneName, SceneCreator creator);

private:
	// Scene名から生成関数を探す一覧です。
	std::unordered_map<std::string, SceneCreator> creators_;
	// 登録順を保つ、表示用のScene名一覧です。
	std::vector<std::string> sceneNames_;
};

