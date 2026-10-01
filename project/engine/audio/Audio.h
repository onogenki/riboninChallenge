#pragma once

#include <vector>
#include <string>
#include <windows.h>
#include <cstdint>
#include <xaudio2.h>
#include <wrl.h>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <mutex>

//音声データ
struct SoundData {
	//波形フォーマット
	WAVEFORMATEX wfex{};
	//バッファ
	std::vector<BYTE>buffer{};
};

class Audio
{

public:
	// 再生中の音を個別に止めたり音量変更したりするための番号です。
	using VoiceId = uint64_t;
	static constexpr VoiceId kInvalidVoiceId = 0;

	// シングルトンインスタンスの取得
	static Audio* GetInstance() {
		static Audio instance;
		return &instance;
	}

	// コピーコンストラクタと代入演算子を禁止
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;

	void Initialize();
	void Update();

	// 音源を共有キャッシュへ読み込みます。再生は行いません。読込失敗時はfalseです。
	bool LoadFile(const std::string& filename);
	// 単発の効果音を指定音量で再生します。SoundEffectPlayerはAudioMixerのSE音量を渡します。
	bool PlayWave(const std::string& filename, float volume = 1.0f);
	// 指定音を無限ループで再生し、後から操作できるVoice番号を返します。
	VoiceId PlayWaveLoop(const std::string& filename, float volume = 1.0f);
	// 再生中の一つの音だけを停止します。効果音やBGMを互いに止めません。
	bool StopWave(VoiceId voiceId);
	// BGMのフェード処理が使う、個別音量の設定です。
	bool SetVoiceVolume(VoiceId voiceId, float volume);
	// Pause中でもVoiceを破棄せず、同じ再生位置から戻せます。
	bool PauseWave(VoiceId voiceId);
	bool ResumeWave(VoiceId voiceId);
	// 未終了のVoiceが残っているかを返します。Pause中もtrueで、音が出ていることとは別です。
	bool IsVoicePlaying(VoiceId voiceId) const;
	void Unload();

private:
	Audio() = default;
	~Audio() = default;

	// xAudio2の本体をFrameworkから引っ越してきて、このクラスの持ち物にする
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

	std::unordered_map<std::string, SoundData> soundDatas_;
	class SourceVoiceCallback;
	struct ActiveVoice
	{
		VoiceId id = kInvalidVoiceId;
		std::shared_ptr<SourceVoiceCallback> callback;
	};
	// Audioが全Voiceの寿命を管理し、BgmPlayerなどは番号だけを持ちます。
	std::vector<ActiveVoice> sourceVoiceCallbacks_;
	mutable std::mutex sourceVoiceMutex_;
	VoiceId nextVoiceId_ = 1;

	// 単発音とループ音の共通生成を行い、成功時は操作用の番号を返します。
	VoiceId PlayWaveInternal(const std::string& filename, bool isLooping, float volume);
	// 一覧をロックして既存VoiceのCallbackを借ります。未登録番号で空のVoiceは作りません。
	// 保持するのはCallbackの寿命だけです。Voice操作と破棄は従来どおりゲーム側の同じスレッドで行います。
	std::shared_ptr<SourceVoiceCallback> FindVoiceCallback(VoiceId voiceId) const;
	// 完了通知を受けたVoiceを一覧から外して破棄します。FrameworkのUpdateから呼びます。
	void ReleaseFinishedVoices();
};
