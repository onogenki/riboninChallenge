#include "SoundEffectPlayer.h"

#include "Audio.h"
#include "AudioMixer.h"

// SoundEffectPlayerは再生装置を作らず、共通Audioへ効果音の再生を依頼します。
void SoundEffectPlayer::Initialize(Audio* audio)
{
	Clear();
	audio_ = audio;
}

// 登録時に読込まで済ませるため、Jumpなどの再生時にファイル読込が発生しません。
bool SoundEffectPlayer::Register(
	const std::string& effectName,
	const std::string& filename)
{
	if (!audio_ || effectName.empty() || filename.empty() || !audio_->LoadFile(filename)) {
		return false;
	}
	effectFilePaths_[effectName] = filename;
	return true;
}

// ゲーム側はファイルパスではなく、登録した効果音名だけを指定して再生します。
bool SoundEffectPlayer::Play(const std::string& effectName) const
{
	if (!audio_) {
		return false;
	}
	const auto found = effectFilePaths_.find(effectName);
	if (found == effectFilePaths_.end()) {
		return false;
	}
	return audio_->PlayWave(
		found->second,
		AudioMixer::GetInstance()->GetSoundEffectVolume());
}

// 登録済みかを確認し、ゲーム側が存在しない効果音を再生しないようにします。
bool SoundEffectPlayer::IsRegistered(const std::string& effectName) const
{
	return effectFilePaths_.contains(effectName);
}

// 共有Audioが持つ読込済みデータは消さず、Scene固有の名前一覧だけを消します。
void SoundEffectPlayer::Clear()
{
	effectFilePaths_.clear();
}
