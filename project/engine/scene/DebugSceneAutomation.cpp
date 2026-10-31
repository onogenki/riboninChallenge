#include "DebugScene.h"
#include "CaptureManager.h"
#include "DirectXCommon.h"
#include <algorithm>
#include <cstdlib>

namespace {

// 環境変数の文字列を取得し、未設定なら空文字列を返します。
std::string GetEnvironmentString(const char* name)
{
	char* value = nullptr;
	size_t size = 0;
	if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
		return {};
	}

	std::string result(value);
	std::free(value);
	return result;
}

}

// 環境変数が有効な時だけ、Scene内の確認対象をDebug専用テストへ渡して開始します。
void DebugScene::InitializeTimePlaybackSmokeFromEnvironment()
{
	if (GetEnvironmentString("RIBONIN_CHALLENGE_TIME_PLAYBACK_SMOKE") != "1") {
		return;
	}

	const std::vector<SceneEditor::ShelfEntry>& modelLibrary = debugSceneEditor_.GetEntries();
	const auto modelIt = std::find_if(
		modelLibrary.begin(),
		modelLibrary.end(),
		[](const SceneEditor::ShelfEntry& entry)
		{
			return entry.canLoad && !entry.hasAnimation && !entry.isTexture;
		});
	const std::string modelFile =
		modelIt != modelLibrary.end() ? modelIt->fileName : std::string{};

	DebugTimePlaybackSmoke::Start(
		timePlaybackSmoke_,
		DebugUiSmoke::IsEnabled(uiSmoke_),
		modelFile,
		Engine::CaptureManager::GetInstance()->MakeTimestampString(),
		MakeTimePlaybackSmokeContext());
}

// UI自動確認を環境変数から初期化し、有効時だけ操作対象を渡します。
void DebugScene::InitializeUiSmokeFromEnvironment()
{
	const std::string smokeMode = GetEnvironmentString("RIBONIN_CHALLENGE_DEBUG_UI_SMOKE");
	if (smokeMode.empty()) {
		return;
	}

	const bool isAnimationPreviewTest = smokeMode == "walk";
	const bool isHotReloadTest = smokeMode == "walk-scene-reload";
	const bool isAnimationSceneTest = smokeMode == "walk-scene" || isHotReloadTest;
	DebugUiSmoke::Start(
		uiSmoke_,
		Engine::CaptureManager::GetInstance()->MakeTimestampString(),
		isAnimationPreviewTest,
		isAnimationSceneTest,
		isHotReloadTest);
}

// UI変更後のParticle Effectと、Sceneを操作する二種類の自動確認を更新します。
void DebugScene::UpdateAutomation()
{
	DebugUiSmoke::Update(uiSmoke_, MakeUiSmokeContext());
	debugParticleEffects_.UpdateFrame(
		GetParticleEffectPosition(),
		Engine::DirectXCommon::GetInstance()->GetDeltaTime());

	DebugTimePlaybackSmoke::Update(
		timePlaybackSmoke_,
		MakeTimePlaybackSmokeContext(),
		Engine::DirectXCommon::GetInstance()->GetDeltaTime());
}
