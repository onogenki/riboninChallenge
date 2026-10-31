#include "StageBgmPlayback.h"

// Audioの初期化はFrameworkに任せ、Stageの再生開始条件だけを新しくします。
void StageBgmPlayback::Initialize(Engine::Audio* audio)
{
	hasDrawnFrame_ = false;
	hasStarted_ = false;
	player_.Initialize(audio);
}

// Draw完了前に音声を読み込まず、次の本編Updateでだけ開始できるようにします。
void StageBgmPlayback::MarkFrameDrawn()
{
	hasDrawnFrame_ = true;
}

// 直起動とTitle経由の両方で、従来と同じ音源・音量・フェード時間を使います。
void StageBgmPlayback::Update(float deltaTime)
{
	if (!hasDrawnFrame_) {
		return;
	}

	if (!hasStarted_) {
		// 現在ある短い音源を動作確認用にループします。曲を追加したらこのパスだけ差し替えます。
		hasStarted_ = player_.Play("resources/debug/Alarm01.wav", 0.20f, 0.80f);
	}
	player_.Update(deltaTime);
}

// Sceneが解放される前にVoiceを返し、次のSceneへ再生状態を持ち越しません。
void StageBgmPlayback::Stop()
{
	player_.Stop();
}
