#include "BgmPlayer.h"

#include "AudioMixer.h"
#include <algorithm>
#include <cmath>

// BgmPlayerは共通Audioを借りるだけで、Audioの初期化や終了処理は行いません。
void BgmPlayer::Initialize(Audio* audio)
{
	Stop();
	audio_ = audio;
}

// 新しいBGMを一曲だけループ再生し、必要な時だけフェードインします。
bool BgmPlayer::Play(const std::string& filename, float volume, float fadeInSeconds)
{
	if (!audio_ || filename.empty() || !audio_->LoadFile(filename)) {
		return false;
	}

	Stop();
	filename_ = filename;
	targetVolume_ = std::clamp(volume, 0.0f, 1.0f);
	currentVolume_ = fadeInSeconds > 0.0f ? 0.0f : targetVolume_;
	voiceId_ = audio_->PlayWaveLoop(
		filename_,
		currentVolume_ * AudioMixer::GetInstance()->GetBgmVolume());
	if (voiceId_ == Audio::kInvalidVoiceId) {
		filename_.clear();
		return false;
	}

	fadeSpeed_ = fadeInSeconds > 0.0f ? targetVolume_ / fadeInSeconds : 0.0f;
	stopWhenSilent_ = false;
	isPaused_ = false;
	return true;
}

// フェード指定がなければすぐ停止し、指定があればUpdateで少しずつ小さくします。
void BgmPlayer::Stop(float fadeOutSeconds)
{
	if (!audio_ || voiceId_ == Audio::kInvalidVoiceId) {
		return;
	}
	if (fadeOutSeconds <= 0.0f) {
		audio_->StopWave(voiceId_);
		voiceId_ = Audio::kInvalidVoiceId;
		filename_.clear();
		isPaused_ = false;
		return;
	}

	targetVolume_ = 0.0f;
	fadeSpeed_ = currentVolume_ / fadeOutSeconds;
	stopWhenSilent_ = true;
}

// Scene停止中に、BGMだけを現在位置で止めたい時に使います。
void BgmPlayer::Pause()
{
	if (audio_ && voiceId_ != Audio::kInvalidVoiceId && audio_->PauseWave(voiceId_)) {
		isPaused_ = true;
	}
}

// PauseしたBGMを同じ再生位置から再開します。
void BgmPlayer::Resume()
{
	if (audio_ && voiceId_ != Audio::kInvalidVoiceId && audio_->ResumeWave(voiceId_)) {
		isPaused_ = false;
	}
}

// 音量を即時変更した後は、進行中のフェードを終了します。
void BgmPlayer::SetVolume(float volume)
{
	targetVolume_ = std::clamp(volume, 0.0f, 1.0f);
	currentVolume_ = targetVolume_;
	fadeSpeed_ = 0.0f;
	if (audio_ && voiceId_ != Audio::kInvalidVoiceId) {
		ApplyMixedVolume();
	}
}

// フェード中だけ、現在音量を目標へ近付けます。
void BgmPlayer::Update(float deltaTime)
{
	if (!audio_ || voiceId_ == Audio::kInvalidVoiceId || isPaused_) {
		return;
	}
	if (!audio_->IsVoicePlaying(voiceId_)) {
		voiceId_ = Audio::kInvalidVoiceId;
		filename_.clear();
		return;
	}
	if (fadeSpeed_ <= 0.0f) {
		return;
	}

	const float volumeDelta = fadeSpeed_ * (std::max)(deltaTime, 0.0f);
	if (currentVolume_ < targetVolume_) {
		currentVolume_ = (std::min)(currentVolume_ + volumeDelta, targetVolume_);
	} else {
		currentVolume_ = (std::max)(currentVolume_ - volumeDelta, targetVolume_);
	}
	ApplyMixedVolume();

	if (std::abs(currentVolume_ - targetVolume_) > 0.0001f) {
		return;
	}
	fadeSpeed_ = 0.0f;
	if (stopWhenSilent_) {
		Stop();
	}
}

// Voice番号がAudioに残っている間だけ、BGMは再生中です。
bool BgmPlayer::IsPlaying() const
{
	return audio_ && voiceId_ != Audio::kInvalidVoiceId && audio_->IsVoicePlaying(voiceId_);
}

// 曲固有のフェード値と、設定画面で変更するBGM全体音量を合成します。
void BgmPlayer::ApplyMixedVolume()
{
	if (audio_ && voiceId_ != Audio::kInvalidVoiceId) {
		audio_->SetVoiceVolume(
			voiceId_,
			currentVolume_ * AudioMixer::GetInstance()->GetBgmVolume());
	}
}
