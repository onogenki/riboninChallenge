#include "AudioMixer.h"

#include <algorithm>

// BGM設定画面などから受け取る値を安全な音量範囲へそろえます。
void AudioMixer::SetBgmVolume(float volume)
{
	bgmVolume_ = std::clamp(volume, 0.0f, 1.0f);
}

// 効果音設定画面などから受け取る値を安全な音量範囲へそろえます。
void AudioMixer::SetSoundEffectVolume(float volume)
{
	soundEffectVolume_ = std::clamp(volume, 0.0f, 1.0f);
}
