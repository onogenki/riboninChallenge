#pragma once

// BGMと効果音の全体音量を別々に保持する、Audio共通の調整窓口です。
// Sceneは音量の保存先を持たず、再生部品がこのMixerを参照して最終音量を決めます。
class AudioMixer
{
public:
	static AudioMixer* GetInstance()
	{
		static AudioMixer instance;
		return &instance;
	}

	AudioMixer(const AudioMixer&) = delete;
	AudioMixer& operator=(const AudioMixer&) = delete;

	// BGM全体へ掛ける音量を0.0から1.0の範囲で設定します。
	void SetBgmVolume(float volume);
	// 効果音全体へ掛ける音量を0.0から1.0の範囲で設定します。
	void SetSoundEffectVolume(float volume);
	float GetBgmVolume() const { return bgmVolume_; }
	float GetSoundEffectVolume() const { return soundEffectVolume_; }

private:
	AudioMixer() = default;
	~AudioMixer() = default;

	// 曲ごとの指定音量へ最後に掛ける、BGMチャンネルの全体音量です。
	float bgmVolume_ = 1.0f;
	// 効果音ごとの指定音量へ最後に掛ける、SEチャンネルの全体音量です。
	float soundEffectVolume_ = 1.0f;
};
