#include "CaptureManager.h"

#include <Windows.h>
#include <vfw.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>

#pragma comment(lib, "vfw32.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

// AVI・MP4の保存と、動画エンコーダースレッドをまとめる実装ファイル。
// スクリーンショット、リプレイ、ImGuiの操作とは責任を分けて読みやすくする。
bool CaptureManager::BeginRecordingAvi(const std::filesystem::path& filePath, int width, int height, int frameRate)
{
	if (width <= 0 || height <= 0 || frameRate <= 0) {
		return false;
	}
	EndRecordingAvi();

	std::error_code errorCode;
	std::filesystem::create_directories(filePath.parent_path(), errorCode);
	if (errorCode) {
		return false;
	}

	AVIFileInit();
	aviInitialized_ = true;
	PAVIFILE aviFile = nullptr;
	HRESULT result = AVIFileOpenW(&aviFile, filePath.wstring().c_str(), OF_WRITE | OF_CREATE, nullptr);
	if (FAILED(result)) {
		EndRecordingAvi();
		return false;
	}

	AVISTREAMINFOW streamInfo{};
	streamInfo.fccType = streamtypeVIDEO;
	streamInfo.dwScale = 1;
	streamInfo.dwRate = static_cast<DWORD>(frameRate);
	streamInfo.dwSuggestedBufferSize = static_cast<DWORD>(width * height * 4);
	SetRect(&streamInfo.rcFrame, 0, 0, width, height);

	PAVISTREAM aviStream = nullptr;
	result = AVIFileCreateStreamW(aviFile, &aviStream, &streamInfo);
	if (FAILED(result)) {
		AVIFileRelease(aviFile);
		EndRecordingAvi();
		return false;
	}

	BITMAPINFOHEADER bitmapHeader{};
	bitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapHeader.biWidth = width;
	bitmapHeader.biHeight = height;
	bitmapHeader.biPlanes = 1;
	bitmapHeader.biBitCount = 32;
	bitmapHeader.biCompression = BI_RGB;
	bitmapHeader.biSizeImage = static_cast<DWORD>(width * height * 4);
	result = AVIStreamSetFormat(aviStream, 0, &bitmapHeader, sizeof(bitmapHeader));
	if (FAILED(result)) {
		AVIStreamRelease(aviStream);
		AVIFileRelease(aviFile);
		EndRecordingAvi();
		return false;
	}

	recordingAviFile_ = aviFile;
	recordingAviStream_ = aviStream;
	recordingVideoPath_ = filePath;
	recordingVideoWidth_ = width;
	recordingVideoHeight_ = height;
	return true;
}

bool CaptureManager::AppendRecordingFrame(
	const std::vector<unsigned char>& pixels,
	int width,
	int height,
	int frameIndex)
{
	if (!recordingAviStream_ || width <= 0 || height <= 0 || pixels.empty()) {
		return false;
	}

	const std::vector<unsigned char>* framePixels = &pixels;
	std::vector<unsigned char> resizedPixels;
	if (width != recordingVideoWidth_ || height != recordingVideoHeight_) {
		resizedPixels = ResizePixels(pixels, width, height, recordingVideoWidth_, recordingVideoHeight_);
		framePixels = &resizedPixels;
		width = recordingVideoWidth_;
		height = recordingVideoHeight_;
	}
	if (framePixels->size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
		return false;
	}

	const size_t rowSize = static_cast<size_t>(width) * 4;
	std::vector<unsigned char> bottomUpPixels(framePixels->size());
	for (int y = 0; y < height; ++y) {
		const unsigned char* source = framePixels->data() + static_cast<size_t>(height - 1 - y) * rowSize;
		unsigned char* destination = bottomUpPixels.data() + static_cast<size_t>(y) * rowSize;
		std::memcpy(destination, source, rowSize);
	}

	const HRESULT result = AVIStreamWrite(
		static_cast<PAVISTREAM>(recordingAviStream_),
		frameIndex,
		1,
		bottomUpPixels.data(),
		static_cast<LONG>(bottomUpPixels.size()),
		AVIIF_KEYFRAME,
		nullptr,
		nullptr);
	return SUCCEEDED(result);
}

void CaptureManager::EndRecordingAvi()
{
	if (recordingAviStream_) {
		AVIStreamRelease(static_cast<PAVISTREAM>(recordingAviStream_));
		recordingAviStream_ = nullptr;
	}
	if (recordingAviFile_) {
		AVIFileRelease(static_cast<PAVIFILE>(recordingAviFile_));
		recordingAviFile_ = nullptr;
	}
	if (aviInitialized_) {
		AVIFileExit();
		aviInitialized_ = false;
	}
	recordingVideoWidth_ = 0;
	recordingVideoHeight_ = 0;
}

bool CaptureManager::BeginRecordingMp4(
	const std::filesystem::path& filePath,
	int width,
	int height,
	int frameRate)
{
	if (width <= 0 || height <= 0 || frameRate <= 0) {
		return false;
	}
	EndRecordingMp4();

	std::error_code errorCode;
	std::filesystem::create_directories(filePath.parent_path(), errorCode);
	if (errorCode) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFAttributes> writerAttributes;
	HRESULT result = MFCreateAttributes(&writerAttributes, 2);
	if (SUCCEEDED(result)) {
		writerAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
		writerAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);
	}

	Microsoft::WRL::ComPtr<IMFSinkWriter> sinkWriter;
	if (SUCCEEDED(result)) {
		result = MFCreateSinkWriterFromURL(
			filePath.wstring().c_str(), nullptr, writerAttributes.Get(), &sinkWriter);
	}
	if (FAILED(result)) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaType> outputType;
	result = MFCreateMediaType(&outputType);
	if (SUCCEEDED(result)) result = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if (SUCCEEDED(result)) result = outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	const UINT32 bitRate = static_cast<UINT32>((std::clamp)(
		static_cast<unsigned long long>(width) * static_cast<unsigned long long>(height) * 10ULL,
		8'000'000ULL,
		40'000'000ULL));
	if (SUCCEEDED(result)) result = outputType->SetUINT32(MF_MT_AVG_BITRATE, bitRate);
	if (SUCCEEDED(result)) result = outputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	if (SUCCEEDED(result)) result = MFSetAttributeSize(outputType.Get(), MF_MT_FRAME_SIZE, width, height);
	if (SUCCEEDED(result)) result = MFSetAttributeRatio(outputType.Get(), MF_MT_FRAME_RATE, frameRate, 1);
	if (SUCCEEDED(result)) result = MFSetAttributeRatio(outputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

	DWORD streamIndex = 0;
	if (SUCCEEDED(result)) result = sinkWriter->AddStream(outputType.Get(), &streamIndex);

	Microsoft::WRL::ComPtr<IMFMediaType> inputType;
	if (SUCCEEDED(result)) result = MFCreateMediaType(&inputType);
	if (SUCCEEDED(result)) result = inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if (SUCCEEDED(result)) result = inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	if (SUCCEEDED(result)) result = inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	if (SUCCEEDED(result)) result = MFSetAttributeSize(inputType.Get(), MF_MT_FRAME_SIZE, width, height);
	if (SUCCEEDED(result)) result = MFSetAttributeRatio(inputType.Get(), MF_MT_FRAME_RATE, frameRate, 1);
	if (SUCCEEDED(result)) result = MFSetAttributeRatio(inputType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	if (SUCCEEDED(result)) result = inputType->SetUINT32(MF_MT_DEFAULT_STRIDE, static_cast<UINT32>(width * 4));
	if (SUCCEEDED(result)) result = sinkWriter->SetInputMediaType(streamIndex, inputType.Get(), nullptr);
	if (SUCCEEDED(result)) result = sinkWriter->BeginWriting();
	if (FAILED(result)) {
		std::ofstream errorLog(std::filesystem::path("logs") / "capture_mp4_error.log", std::ios::trunc);
		if (errorLog) {
			errorLog << "BeginRecordingMp4 HRESULT=0x"
				<< std::hex << static_cast<unsigned long>(result)
				<< " width=" << std::dec << width
				<< " height=" << height
				<< " fps=" << frameRate
				<< '\n';
		}
		return false;
	}

	recordingMp4SinkWriter_ = sinkWriter.Detach();
	recordingMp4StreamIndex_ = streamIndex;
	recordingMp4SampleDuration_ = 10'000'000LL / frameRate;
	recordingVideoPath_ = filePath;
	recordingVideoWidth_ = width;
	recordingVideoHeight_ = height;
	return true;
}

bool CaptureManager::AppendRecordingFrameMp4(
	const std::vector<unsigned char>& pixels,
	int width,
	int height,
	int frameIndex)
{
	IMFSinkWriter* sinkWriter = static_cast<IMFSinkWriter*>(recordingMp4SinkWriter_);
	if (!sinkWriter || pixels.empty() || width <= 0 || height <= 0) {
		return false;
	}

	const std::vector<unsigned char>* framePixels = &pixels;
	std::vector<unsigned char> resizedPixels;
	if (width != recordingVideoWidth_ || height != recordingVideoHeight_) {
		resizedPixels = ResizePixels(pixels, width, height, recordingVideoWidth_, recordingVideoHeight_);
		framePixels = &resizedPixels;
		width = recordingVideoWidth_;
		height = recordingVideoHeight_;
	}
	const DWORD frameBytes = static_cast<DWORD>(width * height * 4);
	if (framePixels->size() < frameBytes) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaBuffer> mediaBuffer;
	HRESULT result = MFCreateMemoryBuffer(frameBytes, &mediaBuffer);
	BYTE* destination = nullptr;
	DWORD maximumLength = 0;
	if (SUCCEEDED(result)) result = mediaBuffer->Lock(&destination, &maximumLength, nullptr);
	if (SUCCEEDED(result) && maximumLength >= frameBytes) {
		std::memcpy(destination, framePixels->data(), frameBytes);
	} else if (SUCCEEDED(result)) {
		result = E_FAIL;
	}
	if (destination) {
		mediaBuffer->Unlock();
	}
	if (SUCCEEDED(result)) result = mediaBuffer->SetCurrentLength(frameBytes);

	Microsoft::WRL::ComPtr<IMFSample> sample;
	if (SUCCEEDED(result)) result = MFCreateSample(&sample);
	if (SUCCEEDED(result)) result = sample->AddBuffer(mediaBuffer.Get());
	if (SUCCEEDED(result)) result = sample->SetSampleTime(
		static_cast<LONGLONG>(frameIndex) * recordingMp4SampleDuration_);
	if (SUCCEEDED(result)) result = sample->SetSampleDuration(recordingMp4SampleDuration_);
	if (SUCCEEDED(result)) result = sinkWriter->WriteSample(recordingMp4StreamIndex_, sample.Get());
	return SUCCEEDED(result);
}

void CaptureManager::EndRecordingMp4()
{
	IMFSinkWriter* sinkWriter = static_cast<IMFSinkWriter*>(recordingMp4SinkWriter_);
	if (sinkWriter) {
		sinkWriter->Finalize();
		sinkWriter->Release();
		recordingMp4SinkWriter_ = nullptr;
	}
	recordingMp4StreamIndex_ = 0;
	recordingMp4SampleDuration_ = 0;
	recordingVideoWidth_ = 0;
	recordingVideoHeight_ = 0;
}

bool CaptureManager::BeginSelectedVideo(
	const std::filesystem::path& directory,
	const std::string& filePrefix,
	int width,
	int height,
	int frameRate)
{
	EndActiveVideo();
	const std::string timestamp = MakeTimestampString();
	if (selectedVideoFormat_ == VideoFormat::Mp4) {
		smokeTrackActiveVideo_ = smokeEnabled_ && filePrefix == "CG2_";
		recordingVideoPath_ = directory / (filePrefix + timestamp + ".mp4");
		const int mp4Width = (std::max)(2, width & ~1);
		const int mp4Height = (std::max)(2, height & ~1);
		if (BeginRecordingMp4(recordingVideoPath_, mp4Width, mp4Height, frameRate)) {
			activeVideoWriter_ = ActiveVideoWriter::Mp4;
			recordingFramesPerSecond_ = frameRate;
			return true;
		}

		recordingVideoPath_ = directory / (filePrefix + timestamp + ".avi");
		const int aviFallbackFrameRate = (std::min)(frameRate, kAviRecordingFramesPerSecond_);
		if (BeginRecordingAvi(recordingVideoPath_, width, height, aviFallbackFrameRate)) {
			activeVideoWriter_ = ActiveVideoWriter::Avi;
			recordingFramesPerSecond_ = aviFallbackFrameRate;
			lastMessage_ = "MP4 was unavailable, so recording automatically switched to AVI (Stable).";
			return true;
		}
		return false;
	}

	recordingVideoPath_ = directory / (filePrefix + timestamp + ".avi");
	smokeTrackActiveVideo_ = smokeEnabled_ && filePrefix == "CG2_";
	if (!BeginRecordingAvi(recordingVideoPath_, width, height, frameRate)) {
		return false;
	}
	activeVideoWriter_ = ActiveVideoWriter::Avi;
	recordingFramesPerSecond_ = frameRate;
	return true;
}

bool CaptureManager::AppendActiveVideoFrame(
	const std::vector<unsigned char>& pixels,
	int width,
	int height,
	int frameIndex)
{
	bool success = false;
	if (activeVideoWriter_ == ActiveVideoWriter::Mp4) {
		success = AppendRecordingFrameMp4(pixels, width, height, frameIndex);
	} else if (activeVideoWriter_ == ActiveVideoWriter::Avi) {
		success = AppendRecordingFrame(pixels, width, height, frameIndex);
	}
	if (success && smokeTrackActiveVideo_) {
		std::vector<unsigned char> normalized =
			ResizePixels(pixels, width, height, recordingVideoWidth_, recordingVideoHeight_);
		if (!normalized.empty()) {
			if (smokeFirstVideoFrame_.empty()) {
				smokeFirstVideoFrame_ = normalized;
			}
			smokeLastVideoFrame_ = std::move(normalized);
			smokeVideoWidth_ = recordingVideoWidth_;
			smokeVideoHeight_ = recordingVideoHeight_;
		}
	}
	return success;
}

void CaptureManager::StartVideoEncoder()
{
	if (videoEncoderThread_.joinable()) {
		StopVideoEncoder();
	}
	{
		std::lock_guard lock(videoEncodeMutex_);
		videoEncodeJobs_.clear();
		videoEncoderStopping_ = false;
		videoEncoderActiveJobs_ = 0;
	}
	videoEncodeFailed_.store(false);
	videoEncoderThread_ = std::thread(&CaptureManager::VideoEncoderWorker, this);
}

void CaptureManager::StopVideoEncoder()
{
	if (!videoEncoderThread_.joinable()) {
		return;
	}
	WaitForVideoEncoder();
	{
		std::lock_guard lock(videoEncodeMutex_);
		videoEncoderStopping_ = true;
	}
	videoEncodeWorkCondition_.notify_all();
	videoEncoderThread_.join();
	{
		std::lock_guard lock(videoEncodeMutex_);
		videoEncodeJobs_.clear();
		videoEncoderActiveJobs_ = 0;
	}
}

void CaptureManager::WaitForVideoEncoder()
{
	std::unique_lock lock(videoEncodeMutex_);
	videoEncodeIdleCondition_.wait(lock, [this] {
		return videoEncodeJobs_.empty() && videoEncoderActiveJobs_ == 0;
	});
}

bool CaptureManager::QueueVideoFrame(
	std::shared_ptr<const std::vector<unsigned char>> pixels,
	int width,
	int height,
	int frameIndex)
{
	if (!pixels || pixels->empty() || width <= 0 || height <= 0 || videoEncodeFailed_.load()) {
		return false;
	}
	{
		std::lock_guard lock(videoEncodeMutex_);
		if (videoEncoderStopping_ || videoEncodeJobs_.size() >= kVideoMaximumQueuedFrames_) {
			return false;
		}
		videoEncodeJobs_.push_back(VideoEncodeJob{
			std::move(pixels), width, height, frameIndex });
	}
	videoEncodeWorkCondition_.notify_one();
	return true;
}

void CaptureManager::VideoEncoderWorker()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
	const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	while (true) {
		VideoEncodeJob job{};
		{
			std::unique_lock lock(videoEncodeMutex_);
			videoEncodeWorkCondition_.wait(lock, [this] {
				return videoEncoderStopping_ || !videoEncodeJobs_.empty();
			});
			if (videoEncoderStopping_ && videoEncodeJobs_.empty()) {
				break;
			}
			job = std::move(videoEncodeJobs_.front());
			videoEncodeJobs_.pop_front();
			++videoEncoderActiveJobs_;
		}

		const auto encodeStart = std::chrono::steady_clock::now();
		const bool success = AppendActiveVideoFrame(
			*job.pixels, job.width, job.height, job.frameIndex);
		if (smokeEnabled_) {
			smokeVideoWorkerMilliseconds_ += std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - encodeStart).count();
			++smokeVideoWorkerSamples_;
		}

		{
			std::lock_guard lock(videoEncodeMutex_);
			if (!success) {
				videoEncodeFailed_.store(true);
				videoEncodeJobs_.clear();
			}
			--videoEncoderActiveJobs_;
			if (videoEncodeJobs_.empty() && videoEncoderActiveJobs_ == 0) {
				videoEncodeIdleCondition_.notify_all();
			}
		}
	}
	if (SUCCEEDED(comResult)) {
		CoUninitialize();
	}
}

void CaptureManager::EndActiveVideo()
{
	if (activeVideoWriter_ == ActiveVideoWriter::Mp4) {
		EndRecordingMp4();
	} else if (activeVideoWriter_ == ActiveVideoWriter::Avi) {
		EndRecordingAvi();
	} else {
		EndRecordingMp4();
		EndRecordingAvi();
	}
	activeVideoWriter_ = ActiveVideoWriter::None;
	smokeTrackActiveVideo_ = false;
}

bool CaptureManager::DecodeMp4FirstAndLastFrame(
	const std::filesystem::path& filePath,
	std::vector<unsigned char>& firstFrame,
	std::vector<unsigned char>& lastFrame,
	int& width,
	int& height,
	int* frameCount) const
{
	Microsoft::WRL::ComPtr<IMFAttributes> readerAttributes;
	HRESULT result = MFCreateAttributes(&readerAttributes, 1);
	if (SUCCEEDED(result)) {
		result = readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
	}
	if (FAILED(result)) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFSourceReader> reader;
	result = MFCreateSourceReaderFromURL(filePath.wstring().c_str(), readerAttributes.Get(), &reader);
	if (FAILED(result)) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaType> requestedType;
	result = MFCreateMediaType(&requestedType);
	if (SUCCEEDED(result)) result = requestedType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if (SUCCEEDED(result)) result = requestedType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	if (SUCCEEDED(result)) result = reader->SetCurrentMediaType(
		MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, requestedType.Get());
	if (FAILED(result)) {
		return false;
	}

	Microsoft::WRL::ComPtr<IMFMediaType> actualType;
	result = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &actualType);
	UINT32 frameWidth = 0;
	UINT32 frameHeight = 0;
	if (SUCCEEDED(result)) result = MFGetAttributeSize(actualType.Get(), MF_MT_FRAME_SIZE, &frameWidth, &frameHeight);
	if (FAILED(result) || frameWidth == 0 || frameHeight == 0) {
		return false;
	}
	width = static_cast<int>(frameWidth);
	height = static_cast<int>(frameHeight);

	UINT32 rawStride = frameWidth * 4;
	actualType->GetUINT32(MF_MT_DEFAULT_STRIDE, &rawStride);
	const LONG stride = static_cast<LONG>(rawStride);
	const size_t absoluteStride = static_cast<size_t>(stride < 0 ? -stride : stride);
	const size_t tightRowSize = static_cast<size_t>(width) * 4;
	if (absoluteStride < tightRowSize) {
		return false;
	}

	int decodedFrameCount = 0;
	while (true) {
		DWORD flags = 0;
		Microsoft::WRL::ComPtr<IMFSample> sample;
		result = reader->ReadSample(
			MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			0,
			nullptr,
			&flags,
			nullptr,
			&sample);
		if (FAILED(result)) {
			return false;
		}
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
			break;
		}
		if (!sample) {
			continue;
		}
		++decodedFrameCount;

		Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
		result = sample->ConvertToContiguousBuffer(&buffer);
		BYTE* data = nullptr;
		DWORD currentLength = 0;
		if (SUCCEEDED(result)) result = buffer->Lock(&data, nullptr, &currentLength);
		if (FAILED(result) || !data || currentLength < absoluteStride * static_cast<size_t>(height)) {
			if (data) buffer->Unlock();
			return false;
		}

		std::vector<unsigned char> decoded(tightRowSize * static_cast<size_t>(height));
		const BYTE* topRow = stride >= 0
			? data
			: data + absoluteStride * static_cast<size_t>(height - 1);
		for (int y = 0; y < height; ++y) {
			const BYTE* sourceRow = topRow + static_cast<ptrdiff_t>(y) * static_cast<ptrdiff_t>(stride);
			std::memcpy(decoded.data() + static_cast<size_t>(y) * tightRowSize, sourceRow, tightRowSize);
		}
		buffer->Unlock();
		if (firstFrame.empty()) {
			firstFrame = decoded;
		}
		lastFrame = std::move(decoded);
	}
	if (frameCount) {
		*frameCount = decodedFrameCount;
	}
	return !firstFrame.empty() && !lastFrame.empty();
}

double CaptureManager::CalculateFrameError(
	const std::vector<unsigned char>& reference,
	const std::vector<unsigned char>& decoded,
	int width,
	int height,
	bool flipVertically,
	bool swapRedBlue) const
{
	const size_t expectedSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
	if (width <= 0 || height <= 0 || reference.size() < expectedSize || decoded.size() < expectedSize) {
		return 1.0e9;
	}

	double error = 0.0;
	for (int y = 0; y < height; ++y) {
		const int decodedY = flipVertically ? height - 1 - y : y;
		for (int x = 0; x < width; ++x) {
			const size_t referenceOffset =
				(static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
			const size_t decodedOffset =
				(static_cast<size_t>(decodedY) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
			for (size_t channel = 0; channel < 3; ++channel) {
				const size_t decodedChannel = swapRedBlue ? 2 - channel : channel;
				error += std::abs(
					static_cast<int>(reference[referenceOffset + channel]) -
					static_cast<int>(decoded[decodedOffset + decodedChannel]));
			}
		}
	}
	return error / static_cast<double>(static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
}

std::vector<unsigned char> CaptureManager::ResizePixels(
	const std::vector<unsigned char>& pixels,
	int sourceWidth,
	int sourceHeight,
	int targetWidth,
	int targetHeight) const
{
	if (sourceWidth <= 0 || sourceHeight <= 0 || targetWidth <= 0 || targetHeight <= 0 ||
		pixels.size() < static_cast<size_t>(sourceWidth) * static_cast<size_t>(sourceHeight) * 4) {
		return {};
	}
	if (sourceWidth == targetWidth && sourceHeight == targetHeight) {
		return pixels;
	}

	std::vector<unsigned char> resized(static_cast<size_t>(targetWidth) * static_cast<size_t>(targetHeight) * 4);
	std::vector<int> sourceColumns(static_cast<size_t>(targetWidth));
	for (int x = 0; x < targetWidth; ++x) {
		sourceColumns[static_cast<size_t>(x)] =
			(static_cast<long long>(x) * sourceWidth) / targetWidth;
	}
	for (int y = 0; y < targetHeight; ++y) {
		const int sourceY = static_cast<int>(
			(static_cast<long long>(y) * sourceHeight) / targetHeight);
		const unsigned char* sourceRow =
			pixels.data() + static_cast<size_t>(sourceY) * static_cast<size_t>(sourceWidth) * 4;
		unsigned char* targetRow =
			resized.data() + static_cast<size_t>(y) * static_cast<size_t>(targetWidth) * 4;
		for (int x = 0; x < targetWidth; ++x) {
			std::memcpy(
				targetRow + static_cast<size_t>(x) * 4,
				sourceRow + static_cast<size_t>(sourceColumns[static_cast<size_t>(x)]) * 4,
				4);
		}
	}
	return resized;
}
