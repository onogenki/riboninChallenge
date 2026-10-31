#pragma once

#include "BgmPlayer.h"

// Stage画面の初回描画後だけBGMを開始し、一曲分の再生とフェードを所有します。
// Sceneは描画と更新の順番を決め、この部品は音声の開始状態だけを管理します。
class StageBgmPlayback
{
public:
	// 共通Audioを借り、前のStageの描画・再生状態を持ち越さないよう初期化します。
	void Initialize(Engine::Audio* audio);
	// このフレームのStage描画が完了したことを記録し、次のUpdateから再生を許可します。
	void MarkFrameDrawn();
	// 初回描画の後だけBGMを開始し、開始済みなら従来どおりフェードを更新します。
	void Update(float deltaTime);
	// Scene終了時に再生中のVoiceを返却します。
	void Stop();

private:
	Engine::BgmPlayer player_{};
	bool hasDrawnFrame_ = false;
	bool hasStarted_ = false;
};
