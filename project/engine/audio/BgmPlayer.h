#pragma once

#include "Audio.h"
#include <string>

// SceneがBGMを一曲だけ管理するための部品です。
// Audio本体は効果音を含む実際の再生を担当し、このクラスはBGMのループ・音量・フェードだけを担当します。
class BgmPlayer
{
public:
	// Audioは所有しません。Frameworkで初期化済みの共通Audioを渡します。
	void Initialize(Audio* audio);
	// 音源をループ再生します。fadeInSecondsを0より大きくすると無音から徐々に大きくなります。
	bool Play(const std::string& filename, float volume = 1.0f, float fadeInSeconds = 0.0f);
	// fadeOutSecondsを0より大きくすると、音量が0になった後で再生を停止します。
	void Stop(float fadeOutSeconds = 0.0f);
	void Pause();
	void Resume();
	// フェードを終え、曲の指定音量×Mixer音量を次のUpdateを待たずVoiceへ反映します。
	void SetVolume(float volume);
	// フェード中の音量を更新します。SceneのUpdateから一度だけ呼びます。
	void Update(float deltaTime);

	// 未終了のVoiceを保持しているかを返します。Pause中もtrueなのでIsPausedと併せて使います。
	bool IsPlaying() const;
	bool IsPaused() const { return isPaused_; }
	const std::string& GetFilename() const { return filename_; }

private:
	// Frameworkが初期化した共有Audioを借ります。BGM終了時はStopでVoice番号を返却します。
	Audio* audio_ = nullptr;
	// Audioが所有する一曲分の再生番号です。この部品がAudioやVoiceをdeleteすることはありません。
	Audio::VoiceId voiceId_ = Audio::kInvalidVoiceId;
	// 再生を開始できた音源のパスです。
	std::string filename_;
	// Mixer倍率を掛ける前の曲音量と、フェードの目標値・一秒当たりの変化量です。
	float currentVolume_ = 1.0f;
	float targetVolume_ = 1.0f;
	float fadeSpeed_ = 0.0f;
	// フェード終了時にStopを呼ぶかと、Pauseでフェードも止めているかを表します。
	bool stopWhenSilent_ = false;
	bool isPaused_ = false;

	// 曲固有の音量へAudioMixerのBGM全体音量を掛け、実際のVoiceへ設定します。
	void ApplyMixedVolume();
};
