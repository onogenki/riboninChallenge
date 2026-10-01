#include "CaptureManager.h"

#include "DirectXCommon.h"

#include <Windows.h>
#include <wincodec.h>

#include <algorithm>
#include <cstring>

// 直近30秒のリプレイ用リングバッファと、圧縮・動画保存ワーカーをまとめる。
// 通常録画とは別の寿命と非同期処理を持つため、専用ファイルで管理する。
void CaptureManager::UpdateReplayBuffer(
	std::shared_ptr<const std::vector<unsigned char>> pixels,
	int width,
	int height)
{
	if (!pixels || pixels->empty() || width <= 0 || height <= 0) {
		return;
	}

	const float scale = (std::min)(
		1.0f,
		(std::min)(
			static_cast<float>(kReplayWidth_) / static_cast<float>(width),
			static_cast<float>(kReplayHeight_) / static_cast<float>(height)));
	const int targetWidth = (std::max)(2, static_cast<int>(static_cast<float>(width) * scale) & ~1);
	const int targetHeight = (std::max)(2, static_cast<int>(static_cast<float>(height) * scale) & ~1);
	ReplayEncodeJob job{};
	job.sourceWidth = width;
	job.sourceHeight = height;
	job.width = targetWidth;
	job.height = targetHeight;
	job.pixels = std::move(pixels);

	{
		std::lock_guard lock(replayMutex_);
		if (replayTargetWidth_ != targetWidth || replayTargetHeight_ != targetHeight) {
			++replayGeneration_;
			replayFrames_.clear();
			replayEncodeJobs_.clear();
			replayTargetWidth_ = targetWidth;
			replayTargetHeight_ = targetHeight;
		}
		job.generation = replayGeneration_;
		if (replayEncodeJobs_.size() >= kReplayMaximumQueuedFrames_) {
			replayEncodeJobs_.pop_front();
			++replayDroppedFrames_;
		}
		replayEncodeJobs_.push_back(std::move(job));
	}
	replayWorkCondition_.notify_one();
}

void CaptureManager::StartReplayEncoder()
{
	StopReplayEncoder();
	{
		std::lock_guard lock(replayMutex_);
		replayEncoderStopping_ = false;
		replayActiveJobs_ = 0;
	}
	replayEncoderThread_ = std::thread(&CaptureManager::ReplayEncoderWorker, this);
}

void CaptureManager::StopReplayEncoder()
{
	{
		std::lock_guard lock(replayMutex_);
		replayEncoderStopping_ = true;
		replayEncodeJobs_.clear();
	}
	replayWorkCondition_.notify_all();
	if (replayEncoderThread_.joinable()) {
		replayEncoderThread_.join();
	}
}

void CaptureManager::ReplayEncoderWorker()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	while (true) {
		ReplayEncodeJob job{};
		{
			std::unique_lock lock(replayMutex_);
			replayWorkCondition_.wait(lock, [this] {
				return replayEncoderStopping_ || !replayEncodeJobs_.empty();
			});
			if (replayEncoderStopping_) {
				break;
			}
			job = std::move(replayEncodeJobs_.front());
			replayEncodeJobs_.pop_front();
			++replayActiveJobs_;
		}

		ReplayFrame frame{};
		const bool encoded = EncodeReplayFrame(job, frame);
		{
			std::lock_guard lock(replayMutex_);
			if (encoded && job.generation == replayGeneration_) {
				replayFrames_.push_back(std::move(frame));
				const size_t maximumFrameCount = static_cast<size_t>(kReplaySeconds_ * kReplayFramesPerSecond_);
				while (replayFrames_.size() > maximumFrameCount) {
					replayFrames_.pop_front();
				}
			}
			--replayActiveJobs_;
			if (replayEncodeJobs_.empty() && replayActiveJobs_ == 0) {
				replayIdleCondition_.notify_all();
			}
		}
	}
	if (SUCCEEDED(comResult)) {
		CoUninitialize();
	}
}

bool CaptureManager::EncodeReplayFrame(const ReplayEncodeJob& job, ReplayFrame& frame) const
{
	if (job.width <= 0 || job.height <= 0 || job.sourceWidth <= 0 || job.sourceHeight <= 0 ||
		!job.pixels || job.pixels->empty()) {
		return false;
	}
	std::vector<unsigned char> resizedPixels = ResizePixels(
		*job.pixels,
		job.sourceWidth,
		job.sourceHeight,
		job.width,
		job.height);
	if (resizedPixels.empty()) {
		return false;
	}

	DirectX::Image image{};
	image.width = static_cast<size_t>(job.width);
	image.height = static_cast<size_t>(job.height);
	image.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	image.rowPitch = static_cast<size_t>(job.width) * 4;
	image.slicePitch = image.rowPitch * static_cast<size_t>(job.height);
	image.pixels = resizedPixels.data();

	DirectX::Blob blob;
	const auto setQuality = [](IPropertyBag2* propertyBag) {
		PROPBAG2 option{};
		option.pstrName = const_cast<wchar_t*>(L"ImageQuality");
		VARIANT value{};
		VariantInit(&value);
		value.vt = VT_R4;
		value.fltVal = 0.94f;
		propertyBag->Write(1, &option, &value);
		VariantClear(&value);
	};
	const HRESULT result = DirectX::SaveToWICMemory(
		image,
		DirectX::WIC_FLAGS_NONE,
		GUID_ContainerFormatJpeg,
		blob,
		&GUID_WICPixelFormat24bppBGR,
		setQuality);
	if (FAILED(result) || blob.GetBufferSize() == 0) {
		return false;
	}

	frame.width = job.width;
	frame.height = job.height;
	const unsigned char* encoded = static_cast<const unsigned char*>(blob.GetBufferPointer());
	frame.encodedPixels.assign(encoded, encoded + blob.GetBufferSize());
	return true;
}

bool CaptureManager::DecodeReplayFrame(const ReplayFrame& frame, std::vector<unsigned char>& pixels) const
{
	if (frame.width <= 0 || frame.height <= 0 || frame.encodedPixels.empty()) {
		return false;
	}

	DirectX::ScratchImage decoded;
	HRESULT result = DirectX::LoadFromWICMemory(
		frame.encodedPixels.data(),
		frame.encodedPixels.size(),
		DirectX::WIC_FLAGS_NONE,
		nullptr,
		decoded);
	if (FAILED(result) || !decoded.GetImage(0, 0, 0)) {
		return false;
	}

	const DirectX::Image* source = decoded.GetImage(0, 0, 0);
	DirectX::ScratchImage converted;
	if (source->format != DXGI_FORMAT_B8G8R8A8_UNORM) {
		result = DirectX::Convert(
			*source,
			DXGI_FORMAT_B8G8R8A8_UNORM,
			DirectX::TEX_FILTER_DEFAULT,
			DirectX::TEX_THRESHOLD_DEFAULT,
			converted);
		if (FAILED(result) || !converted.GetImage(0, 0, 0)) {
			return false;
		}
		source = converted.GetImage(0, 0, 0);
	}

	const size_t tightRowPitch = static_cast<size_t>(frame.width) * 4;
	pixels.resize(tightRowPitch * static_cast<size_t>(frame.height));
	for (int y = 0; y < frame.height; ++y) {
		std::memcpy(
			pixels.data() + static_cast<size_t>(y) * tightRowPitch,
			source->pixels + static_cast<size_t>(y) * source->rowPitch,
			tightRowPitch);
	}
	return true;
}

void CaptureManager::ClearReplayBuffer()
{
	std::lock_guard lock(replayMutex_);
	++replayGeneration_;
	replayFrames_.clear();
	replayEncodeJobs_.clear();
	replayTargetWidth_ = 0;
	replayTargetHeight_ = 0;
	replayFrameTimer_ = 0.0f;
}

bool CaptureManager::SaveReplayClip()
{
	FinishReplaySaveIfReady();
	if (isRecording_ || recordingStartRequested_ || replaySaveInProgress_.load() || !CreateCaptureDirectories()) {
		return false;
	}

	std::vector<ReplayFrame> frames;
	{
		std::unique_lock lock(replayMutex_);
		replayIdleCondition_.wait(lock, [this] {
			return replayEncodeJobs_.empty() && replayActiveJobs_ == 0;
		});
		if (replayFrames_.empty()) {
			return false;
		}
		frames.assign(replayFrames_.begin(), replayFrames_.end());
		if (smokeEnabled_) {
			smokeReplayFrameCount_ = static_cast<int>(frames.size());
			smokeReplayDroppedFrames_ = replayDroppedFrames_;
		}
	}
	if (smokeEnabled_ &&
		(!DecodeReplayFrame(frames.front(), smokeFirstReplayFrame_) ||
		 !DecodeReplayFrame(frames.back(), smokeLastReplayFrame_))) {
		return false;
	}

	replaySaveSucceeded_ = false;
	replaySaveInProgress_.store(true);
	replaySaveThread_ = std::thread([this, frames = std::move(frames)]() mutable {
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		replaySaveSucceeded_ = SaveReplayFrames(std::move(frames));
		replaySaveInProgress_.store(false);
		if (SUCCEEDED(comResult)) {
			CoUninitialize();
		}
	});
	return true;
}

bool CaptureManager::SaveReplayFrames(std::vector<ReplayFrame> frames)
{
	if (frames.empty()) {
		return false;
	}

	const ReplayFrame& firstFrame = frames.front();
	recordingFrameIndex_ = 0;
	if (!BeginSelectedVideo(
		GetCaptureDirectory("Replays"),
		"CG2_replay_",
		firstFrame.width,
		firstFrame.height,
		kReplayFramesPerSecond_)) {
		return false;
	}

	bool success = true;
	std::vector<unsigned char> decodedPixels;
	for (const ReplayFrame& frame : frames) {
		if (!DecodeReplayFrame(frame, decodedPixels) ||
			!AppendActiveVideoFrame(decodedPixels, frame.width, frame.height, recordingFrameIndex_)) {
			success = false;
			break;
		}
		++recordingFrameIndex_;
	}
	EndActiveVideo();
	if (!success) {
		return false;
	}

	lastReplayPath_ = recordingVideoPath_;
	lastVideoPath_ = recordingVideoPath_;
	return true;
}

void CaptureManager::WaitForReplaySave()
{
	if (replaySaveThread_.joinable()) {
		replaySaveThread_.join();
	}
	replaySaveInProgress_.store(false);
}

void CaptureManager::FinishReplaySaveIfReady()
{
	if (replaySaveInProgress_.load() || !replaySaveThread_.joinable()) {
		return;
	}
	replaySaveThread_.join();
	lastMessage_ = replaySaveSucceeded_
		? "Saved replay: " + lastReplayPath_.string()
		: "Replay save failed.";
}

