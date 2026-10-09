#pragma once

#include <string>

// 撮影や入力を行わず、Loading開始後にテストが予約するScene切替とDrawだけを確認します。
// 現在のTitleのEnterは同じStage1内で進むため、このテストの切替経路とは異なります。
class StartupRouteSmoke
{
public:
	// 環境変数が指定された場合だけ有効にし、直接Scene起動との混同を防ぎます。
	void Initialize(bool requested, bool startsWithLoading);
	// SceneManagerが現在Sceneを更新した後に、次のSceneへの要求と成否を調べます。
	void Update();
	// GameのDrawが終わった後に呼び、各Sceneが少なくとも一度描画されたことを記録します。
	void AfterDraw();
	bool IsFinished() const { return finished_; }
	bool WasSuccessful() const { return wasSuccessful_; }

private:
	// 結果をlogsへ一度だけ書き、Gameが終了要求を出せる状態にします。
	void Finish(bool success, const std::string& reason);

	bool enabled_ = false;
	bool finished_ = false;
	bool wasSuccessful_ = false;
	bool stage1Requested_ = false;
	std::string currentSceneName_;
	int totalUpdateCount_ = 0;
	int sceneUpdateCount_ = 0;
	int loadingVisitCount_ = 0;
	int firstLoadingDrawCount_ = 0;
	int secondLoadingDrawCount_ = 0;
	int titleDrawCount_ = 0;
	int stage1DrawCount_ = 0;
};
