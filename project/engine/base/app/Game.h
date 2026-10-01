#pragma once

#include "Framework.h"
#include "StartupRouteSmoke.h"
#include <string>

class Game : public Framework
{
public:

	// Frameworkのライフサイクルに合わせてゲーム全体を管理する。
	void Initialize() override;
	void Finalize()override;
	void Update()override;
	void Draw()override;

private:

	// 環境変数で指定されたシーン再起動テストの設定を読む。
	void InitializeSceneStressFromEnvironment();
	// 有効時は一定フレームごとにシーンを切り替え、初期化・破棄を検証する。
	void UpdateSceneStress();

	// 起動順だけの自動確認をGameから呼び、撮影テストとは独立させます。
	StartupRouteSmoke startupRouteSmoke_;

	bool sceneStressEnabled_ = false;
	std::string sceneStressTarget_ = "DEBUG";
	int sceneStressRequestedRestarts_ = 0;
	int sceneStressCompletedRestarts_ = 0;
	int sceneStressIntervalFrames_ = 8;
	int sceneStressFrameCounter_ = 0;
};

