#pragma once

#include "BaseScene.h"
#include <memory>
#include <string>

namespace Engine { class Sprite; }
namespace Engine { class SpriteCommon; }

// 本編を作る間だけ白い画面を表示し、編集用Sceneを起動中に見せないためのSceneです。
class LoadingScene : public BaseScene
{
public:
	LoadingScene();
	~LoadingScene() override;

	// 白い画面を描くSpriteだけを準備します。
	void Initialize() override;
	// LoadingSceneが所有する白背景Spriteを解放します。
	void Finalize() override;
	// 一度白画面を表示してから次Sceneを予約し、生成不能なら失敗を通知します。
	void Update() override;
	// ImGuiを出さず、白いLoading画面だけを描画します。
	void Draw() override;

private:
	// 白背景の描画設定はManagerから借り、LoadingSceneだけで使います。
	Engine::SpriteCommon* spriteCommon = nullptr;

	// 白画像が読めた時だけ、画面全体を覆うSpriteを作ります。
	bool InitializeLoadingSprite();

	// 起動中に表示する白背景です。
	std::unique_ptr<Engine::Sprite> loadingSprite_;
	// SceneManagerから一度だけ受け取り、切替予約に成功するまで保持する次Scene名です。
	std::string nextSceneName_;
	// 次Sceneへの切替予約を一回だけ行うためのフラグです。
	bool hasRequestedNextScene_ = false;
};
