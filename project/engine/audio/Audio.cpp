#include "Audio.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl.h>
#include <algorithm>
#include <cassert>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

using namespace Microsoft::WRL;

class Audio::SourceVoiceCallback final : public IXAudio2VoiceCallback {
public:
	void SetVoice(IXAudio2SourceVoice* voice) { voice_ = voice; }
	IXAudio2SourceVoice* GetVoice() const { return voice_; }
	bool IsFinished() const { return isFinished_.load(std::memory_order_acquire); }
	void MarkFinished() { isFinished_.store(true, std::memory_order_release); }

	void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
	void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
	void STDMETHODCALLTYPE OnStreamEnd() override { MarkFinished(); }
	void STDMETHODCALLTYPE OnBufferStart(void*) override {}
	void STDMETHODCALLTYPE OnBufferEnd(void*) override { MarkFinished(); }
	void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
	void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override { MarkFinished(); }

private:
	IXAudio2SourceVoice* voice_ = nullptr;
	std::atomic_bool isFinished_ = false;
};

std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

void Audio::Initialize()
{
	//初期化処理
	HRESULT result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(result)) {
		return;
	}

	result = xAudio2_->CreateMasteringVoice(&masterVoice_);
	if (FAILED(result)) {
		xAudio2_.Reset();
		return;
	}

	result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	if (FAILED(result)) {
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
		xAudio2_.Reset();
	}
}

// 音源をPCMへ読み込み、成功したデータだけを共有キャッシュへ登録します。ここでは再生しません。
bool Audio::LoadFile(const std::string& filename)
{

	// 既に読み込み済みなら何もしない（二重読み込み防止）
	if (soundDatas_.find(filename) != soundDatas_.end()) {
		return true;
	}

	HRESULT result;

	// ファイルパスをワイド文字列に変換
	std::wstring filePathW = ConvertString(filename);

	//SourceReader作成
	ComPtr<IMFSourceReader> pReader;
	result = MFCreateSourceReaderFromURL(filePathW.c_str(), nullptr, &pReader);
	if (FAILED(result)) {
		return false;
	}

	//PCM形式にフォーマット指定する
	// Media Foundationの特殊なストリーム番号を、APIが受け取るDWORDとして明示します。
	const DWORD audioStreamIndex = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
	ComPtr<IMFMediaType> pPCMType;
	result = MFCreateMediaType(&pPCMType);
	if (FAILED(result)) {
		return false;
	}
	pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	result = pReader->SetCurrentMediaType(audioStreamIndex, nullptr, pPCMType.Get());
	if (FAILED(result)) {
		return false;
	}

	//実際にセットされたメディアタイプを取得する
	ComPtr<IMFMediaType>pOutType;
	result = pReader->GetCurrentMediaType(audioStreamIndex, &pOutType);
	if (FAILED(result)) {
		return false;
	}

	//Waveフォーマットを取得する
	WAVEFORMATEX* waveFormat = nullptr;
	result = MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &waveFormat, nullptr);
	if (FAILED(result) || !waveFormat) {
		return false;
	}

	//コンテナに格納する音声データ
	SoundData soundData = {};
	soundData.wfex = *waveFormat;
	//生成したWaveフォーマットを開放
	CoTaskMemFree(waveFormat);

	while (true)
	{
		ComPtr<IMFSample> pSample;
		DWORD streamIndex = 0, flags = 0;
		LONGLONG llTimeStamp = 0;
		//サンプルを読み込む
		result = pReader->ReadSample(audioStreamIndex, 0, &streamIndex, &flags, &llTimeStamp, &pSample);
		if (FAILED(result)) {
			return false;
		}
		//ストリームの末尾に達したら抜ける
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)break;

		if (pSample)
		{
			ComPtr<IMFMediaBuffer> pBuffer;
			//サンプルに含まれるサウンドデータのバッファを一繋ぎにして取得
			pSample->ConvertToContiguousBuffer(&pBuffer);

			BYTE* pData = nullptr;//データ読み取りポインタ
			DWORD maxLength = 0, currentLength = 0;
			//バッファの読み込み時にロック
			pBuffer->Lock(&pData, &maxLength, &currentLength);
			//バッファの末尾にデータを追加
			soundData.buffer.insert(soundData.buffer.end(), pData, pData + currentLength);
			pBuffer->Unlock();
		}
	}

	soundDatas_[filename] = soundData;
	return true;
}

void Audio::Update()
{
	ReleaseFinishedVoices();
}

//音声データ解放
void Audio::Unload()
{
	std::vector<ActiveVoice> activeVoices;
	{
		std::scoped_lock lock(sourceVoiceMutex_);
		activeVoices.swap(sourceVoiceCallbacks_);
	}
	for (const ActiveVoice& activeVoice : activeVoices) {
		const std::shared_ptr<SourceVoiceCallback>& callback = activeVoice.callback;
		if (!callback || !callback->GetVoice()) {
			continue;
		}
		IXAudio2SourceVoice* voice = callback->GetVoice();
		voice->Stop();
		voice->DestroyVoice();
	}
	//バッファのメモリを解放
	soundDatas_.clear();
}

// 単発の効果音は、従来どおり再生終了後に自動で破棄します。
bool Audio::PlayWave(const std::string& filename, float volume)
{
	return PlayWaveInternal(filename, false, volume) != kInvalidVoiceId;
}

// BGM用に、停止されるまで同じ音を繰り返すVoiceを作成します。
Audio::VoiceId Audio::PlayWaveLoop(const std::string& filename, float volume)
{
	return PlayWaveInternal(filename, true, volume);
}

// 単発音とループ音のどちらにも共通する、XAudio2 Voice生成処理です。
Audio::VoiceId Audio::PlayWaveInternal(
	const std::string& filename,
	bool isLooping,
	float volume)
{
	HRESULT result;

	// 本棚から音声データを探し出す
	auto soundIt = soundDatas_.find(filename);
	if (!xAudio2_ || soundIt == soundDatas_.end()) {
		return kInvalidVoiceId;
	}
	SoundData& soundData = soundIt->second;

	//波形フォーマットを先にSourceVoiceの生成
	auto callback = std::make_shared<SourceVoiceCallback>();
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex, 0, XAUDIO2_DEFAULT_FREQ_RATIO, callback.get());
	if (FAILED(result)) {
		return kInvalidVoiceId;
	}
	callback->SetVoice(pSourceVoice);

	//再生する波形データの設定
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.buffer.data();
	buf.AudioBytes = static_cast<UINT32>(soundData.buffer.size());
	buf.Flags = isLooping ? 0 : XAUDIO2_END_OF_STREAM;
	buf.LoopCount = isLooping ? XAUDIO2_LOOP_INFINITE : 0;

	//波形データの再生
	result = pSourceVoice->SubmitSourceBuffer(&buf);
	if (FAILED(result)) {
		pSourceVoice->DestroyVoice();
		return kInvalidVoiceId;
	}
	pSourceVoice->SetVolume(std::clamp(volume, 0.0f, 1.0f));
	VoiceId voiceId = kInvalidVoiceId;
	{
		std::scoped_lock lock(sourceVoiceMutex_);
		voiceId = nextVoiceId_++;
		sourceVoiceCallbacks_.push_back({ voiceId, callback });
	}
	result = pSourceVoice->Start();
	if (FAILED(result)) {
		callback->MarkFinished();
		ReleaseFinishedVoices();
		return kInvalidVoiceId;
	}
	return voiceId;
}

// 指定番号のVoiceだけを止め、ほかの効果音やBGMは残します。
bool Audio::StopWave(VoiceId voiceId)
{
	std::shared_ptr<SourceVoiceCallback> callback;
	{
		std::scoped_lock lock(sourceVoiceMutex_);
		const auto found = std::find_if(
			sourceVoiceCallbacks_.begin(),
			sourceVoiceCallbacks_.end(),
			[voiceId](const ActiveVoice& activeVoice) { return activeVoice.id == voiceId; });
		if (found == sourceVoiceCallbacks_.end()) {
			return false;
		}
		callback = found->callback;
		sourceVoiceCallbacks_.erase(found);
	}
	if (!callback || !callback->GetVoice()) {
		return false;
	}
	callback->GetVoice()->Stop();
	callback->GetVoice()->FlushSourceBuffers();
	callback->GetVoice()->DestroyVoice();
	return true;
}

// Voice番号を使って、再生途中でも音量を変更します。
bool Audio::SetVoiceVolume(VoiceId voiceId, float volume)
{
	const std::shared_ptr<SourceVoiceCallback> callback = FindVoiceCallback(voiceId);
	return callback && callback->GetVoice() &&
		SUCCEEDED(callback->GetVoice()->SetVolume(std::clamp(volume, 0.0f, 1.0f)));
}

// 一時停止はVoiceを破棄せず、ResumeWaveで同じ位置から再開します。
bool Audio::PauseWave(VoiceId voiceId)
{
	const std::shared_ptr<SourceVoiceCallback> callback = FindVoiceCallback(voiceId);
	return callback && callback->GetVoice() && SUCCEEDED(callback->GetVoice()->Stop());
}

// PauseWaveで止めたVoiceを、先頭からではなく停止位置から再開します。
bool Audio::ResumeWave(VoiceId voiceId)
{
	const std::shared_ptr<SourceVoiceCallback> callback = FindVoiceCallback(voiceId);
	return callback && callback->GetVoice() && SUCCEEDED(callback->GetVoice()->Start());
}

// 効果音が終了済み、またはStopWave済みならfalseを返します。
bool Audio::IsVoicePlaying(VoiceId voiceId) const
{
	std::scoped_lock lock(sourceVoiceMutex_);
	const auto found = std::find_if(
		sourceVoiceCallbacks_.begin(),
		sourceVoiceCallbacks_.end(),
		[voiceId](const ActiveVoice& activeVoice) { return activeVoice.id == voiceId; });
	return found != sourceVoiceCallbacks_.end() &&
		found->callback && !found->callback->IsFinished();
}

// 非破壊のVoice操作で同じ番号検索を使い、未登録ならnullptrで呼び出し元へ返します。
std::shared_ptr<Audio::SourceVoiceCallback> Audio::FindVoiceCallback(VoiceId voiceId) const
{
	std::scoped_lock lock(sourceVoiceMutex_);
	const auto found = std::find_if(
		sourceVoiceCallbacks_.begin(),
		sourceVoiceCallbacks_.end(),
		[voiceId](const ActiveVoice& activeVoice) { return activeVoice.id == voiceId; });
	return found != sourceVoiceCallbacks_.end() ? found->callback : nullptr;
}

// Callbackは終了フラグだけを立て、実際のVoice破棄はゲーム側のUpdateでまとめて行います。
void Audio::ReleaseFinishedVoices()
{
	std::vector<std::shared_ptr<SourceVoiceCallback>> callbacksToRelease;
	{
		std::scoped_lock lock(sourceVoiceMutex_);
		for (auto it = sourceVoiceCallbacks_.begin(); it != sourceVoiceCallbacks_.end();) {
			if (it->callback && it->callback->IsFinished()) {
				callbacksToRelease.push_back(std::move(it->callback));
				it = sourceVoiceCallbacks_.erase(it);
			} else {
				++it;
			}
		}
	}

	for (const auto& callback : callbacksToRelease) {
		callback->GetVoice()->DestroyVoice();
	}
}
