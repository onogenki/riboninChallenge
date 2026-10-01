#include "Audio.h"
#include "AudioMixer.h"
#include "BgmPlayer.h"
#include "SoundEffectPlayer.h"
#include <mfapi.h>
#include <iostream>

namespace
{
	// assertに頼らず、NDEBUGの構成でも全てのAPIを実行して失敗を記録します。
	void Check(bool result, const char* name, int& failures)
	{
		std::cout << (result ? "PASS: " : "FAIL: ") << name << '\n';
		if (!result) {
			++failures;
		}
	}
}

// 画面とSceneを起動せず、実際のAudioを無音で操作します。聴感や再生タイミングのテストではありません。
int main(int argc, char** argv)
{
	if (argc != 2) {
		std::cerr << "Usage: AudioApiSmoke <existing audio file>\n";
		return 2;
	}
	const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(comResult)) {
		std::cerr << "COM initialization failed\n";
		return 2;
	}

	int failures = 0;
	Audio* audio = Audio::GetInstance();
	AudioMixer* mixer = AudioMixer::GetInstance();
	audio->Initialize();
	Check(!audio->SetVoiceVolume(Audio::kInvalidVoiceId, 0.0f), "invalid volume target", failures);
	Check(!audio->PauseWave(Audio::kInvalidVoiceId), "invalid pause target", failures);
	Check(!audio->ResumeWave(Audio::kInvalidVoiceId), "invalid resume target", failures);
	Check(!audio->StopWave(Audio::kInvalidVoiceId), "invalid stop target", failures);
	Check(!audio->IsVoicePlaying(Audio::kInvalidVoiceId), "invalid playing query", failures);

	const std::string filename = argv[1];
	Check(audio->LoadFile(filename), "load existing audio", failures);
	Check(audio->LoadFile(filename), "reuse loaded audio", failures);
	const Audio::VoiceId voiceId = audio->PlayWaveLoop(filename, 0.0f);
	Check(voiceId != Audio::kInvalidVoiceId, "create silent loop", failures);
	Check(audio->SetVoiceVolume(voiceId, 0.0f), "set existing voice volume", failures);
	Check(audio->PauseWave(voiceId), "pause existing voice", failures);
	Check(audio->IsVoicePlaying(voiceId), "paused voice is retained", failures);
	Check(audio->ResumeWave(voiceId), "resume existing voice", failures);
	Check(audio->StopWave(voiceId), "stop existing voice", failures);
	Check(!audio->SetVoiceVolume(voiceId, 0.0f), "stopped voice rejects volume", failures);
	Check(!audio->PauseWave(voiceId), "stopped voice rejects pause", failures);
	Check(!audio->ResumeWave(voiceId), "stopped voice rejects resume", failures);
	Check(!audio->IsVoicePlaying(voiceId), "stopped voice is removed", failures);

	// 全体音量を0へそろえ、実機テストでもBGM・SEの音を出しません。
	mixer->SetBgmVolume(-1.0f);
	mixer->SetSoundEffectVolume(2.0f);
	Check(mixer->GetBgmVolume() == 0.0f, "BGM volume lower limit", failures);
	Check(mixer->GetSoundEffectVolume() == 1.0f, "SE volume upper limit", failures);
	mixer->SetSoundEffectVolume(0.0f);
	BgmPlayer bgm;
	bgm.Initialize(audio);
	Check(bgm.Play(filename, 0.2f, 0.1f), "BGM start", failures);
	bgm.Update(0.1f);
	bgm.Pause();
	Check(bgm.IsPlaying() && bgm.IsPaused(), "BGM pause state", failures);
	bgm.Resume();
	Check(bgm.IsPlaying() && !bgm.IsPaused(), "BGM resume state", failures);
	bgm.SetVolume(0.1f);
	bgm.Stop(0.1f);
	bgm.Update(0.2f);
	Check(!bgm.IsPlaying() && bgm.GetFilename().empty(), "BGM fade-out stop", failures);

	SoundEffectPlayer effects;
	effects.Initialize(audio);
	Check(!effects.Play("unregistered"), "unknown SE rejected", failures);
	Check(effects.Register("test", filename), "SE registration", failures);
	Check(effects.IsRegistered("test") && effects.Play("test"), "registered silent SE", failures);
	effects.Clear();
	Check(!effects.IsRegistered("test") && !effects.Play("test"), "SE names cleared", failures);

	// ゲームと同様に、再生部品を止めてから共有Audioの音源を解放します。
	bgm.Stop();
	audio->Update();
	audio->Unload();
	MFShutdown();
	CoUninitialize();
	std::cout << (failures == 0 ? "SUCCESS" : "FAILURE") << ": failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
