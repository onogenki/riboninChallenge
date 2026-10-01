#include "StartupRouteSmoke.h"

#include "SceneManager.h"
#include <fstream>

// 通常起動以外を指定したテストは、誤って成功扱いにしないで停止します。
void StartupRouteSmoke::Initialize(bool requested, bool startsWithLoading)
{
	enabled_ = requested;
	if (enabled_ && !startsWithLoading) {
		Finish(false, "CG2_START_SCENE must be unset.");
	}
}

// Loading→Title→Loading→Stage1へ進め、途中のScene切替失敗は次フレームで再試行します。
void StartupRouteSmoke::Update()
{
	if (!enabled_ || finished_) {
		return;
	}

	SceneManager* sceneManager = SceneManager::GetInstance();
	const std::string& sceneName = sceneManager->GetCurrentSceneName();
	if (sceneName != currentSceneName_) {
		currentSceneName_ = sceneName;
		sceneUpdateCount_ = 0;
		if (sceneName == "LOADING") {
			++loadingVisitCount_;
		}
	}
	++sceneUpdateCount_;
	++totalUpdateCount_;

	if (sceneName == "TITLE" && sceneUpdateCount_ >= 8 &&
		titleDrawCount_ > 0 && !stage1Requested_ && !sceneManager->HasPendingScene()) {
		stage1Requested_ = sceneManager->ChangeSceneWithLoading("STAGE1");
	}
	if (sceneName == "STAGE1" && sceneUpdateCount_ >= 8 && stage1DrawCount_ > 0) {
		const bool passed =
			loadingVisitCount_ == 2 &&
			firstLoadingDrawCount_ > 0 &&
			secondLoadingDrawCount_ > 0 &&
			titleDrawCount_ > 0;
		Finish(passed, passed ? "All four scenes drew in order." : "A scene was skipped or not drawn.");
	} else if (totalUpdateCount_ >= 600) {
		Finish(false, "The route did not reach Stage1 within 600 updates.");
	}
}

// 更新しただけでは画面に出た証拠にならないため、Draw後にScene別の回数を数えます。
void StartupRouteSmoke::AfterDraw()
{
	if (!enabled_ || finished_) {
		return;
	}
	const std::string& sceneName = SceneManager::GetInstance()->GetCurrentSceneName();
	if (sceneName == "LOADING" && loadingVisitCount_ == 1) {
		++firstLoadingDrawCount_;
	} else if (sceneName == "LOADING" && loadingVisitCount_ == 2) {
		++secondLoadingDrawCount_;
	} else if (sceneName == "TITLE") {
		++titleDrawCount_;
	} else if (sceneName == "STAGE1") {
		++stage1DrawCount_;
	}
}

// ログには順番と描画回数を残し、単なる起動成功と区別できるようにします。
void StartupRouteSmoke::Finish(bool success, const std::string& reason)
{
	if (finished_) {
		return;
	}
	std::ofstream log("logs/startup_route_smoke.log", std::ios::trunc);
	if (log) {
		log << (success ? "SUCCESS" : "FAILURE") << ": " << reason
			<< " loadingVisits=" << loadingVisitCount_
			<< " firstLoadingDraws=" << firstLoadingDrawCount_
			<< " titleDraws=" << titleDrawCount_
			<< " secondLoadingDraws=" << secondLoadingDrawCount_
			<< " stage1Draws=" << stage1DrawCount_ << '\n';
	}
	finished_ = true;
}
