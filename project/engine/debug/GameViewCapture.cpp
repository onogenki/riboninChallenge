#include "GameViewCapture.h"

#include "DirectXCommon.h"
#include "PostEffect.h"
#include <Windows.h>
#include <shellapi.h>
#include <vfw.h>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif
#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "vfw32.lib")

namespace
{
	bool OpenFolderInExplorer(const std::filesystem::path& directory)
	{
		std::error_code errorCode;
		std::filesystem::create_directories(directory, errorCode);
		if (errorCode) {
			return false;
		}

		const HINSTANCE result = ShellExecuteW(
			nullptr,
			L"open",
			directory.wstring().c_str(),
			nullptr,
			nullptr,
			SW_SHOWNORMAL);
		return reinterpret_cast<INT_PTR>(result) > 32;
	}
}

void GameViewCapture::Initialize()
{
	CreateDirectories();
}

void GameViewCapture::Finalize()
{
	EndRecordingAvi();
	isRecording_ = false;
	recordingTime_ = 0.0f;
	recordingFrameTimer_ = 0.0f;
	recordingFrameIndex_ = 0;
	recordingVideoPath_.clear();
	lastScreenshotPath_.clear();
	lastVideoPath_.clear();
	lastReplayPath_.clear();
	replayFrames_.clear();
	replayFrameTimer_ = 0.0f;
	lastMessage_.clear();
}

void GameViewCapture::Update()
{
	UpdateRecording();
	UpdateReplayBuffer();
}

std::filesystem::path GameViewCapture::GetCaptureDirectory(const char* folderName) const
{
	std::error_code errorCode;
	const std::filesystem::path resourceDirectory = std::filesystem::absolute("resources", errorCode);
	return (errorCode ? std::filesystem::path("resources") : resourceDirectory) / "Captures" / folderName;
}

bool GameViewCapture::CreateDirectories() const
{
	std::error_code errorCode;
	for (const char* folderName : { "Screenshots", "Videos", "Replays" }) {
		std::filesystem::create_directories(GetCaptureDirectory(folderName), errorCode);
		if (errorCode) {
			return false;
		}
	}
	return true;
}

std::string GameViewCapture::MakeTimestampString() const
{
	const auto now = std::chrono::system_clock::now();
	const std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
	std::tm localTime{};
	localtime_s(&localTime, &currentTime);
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::ostringstream stream;
	stream << std::put_time(&localTime, "%Y%m%d_%H%M%S")
		<< '_' << std::setw(3) << std::setfill('0') << milliseconds.count();
	return stream.str();
}

bool GameViewCapture::CapturePixels(std::vector<unsigned char>& pixels, int& width, int& height)
{
#ifdef USE_IMGUI
	// ImGui表示用のGame View Textureから、RGBA画素をCPU側のvectorへコピーします。
	return DirectXCommon::GetInstance()->CaptureGameTexturePixels(
		pixels,
		width,
		height,
		PostEffect::GetInstance()->IsEnabled());
#else
	(void)pixels;
	(void)width;
	(void)height;
	return false;
#endif
}

bool GameViewCapture::SavePixelsAsBmp(
	const std::filesystem::path& filePath,
	const std::vector<unsigned char>& pixels,
	int width,
	int height) const
{
	// BMPは先頭に二種類のHeaderを付けてから、RGBA32bitの画素をそのまま保存します。
	if (pixels.empty() || width <= 0 || height <= 0) {
		return false;
	}

	std::error_code errorCode;
	std::filesystem::create_directories(filePath.parent_path(), errorCode);
	if (errorCode) {
		return false;
	}

	const DWORD imageSize = static_cast<DWORD>(width * height * 4);
	BITMAPFILEHEADER fileHeader{};
	fileHeader.bfType = 0x4D42;
	fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	fileHeader.bfSize = fileHeader.bfOffBits + imageSize;

	BITMAPINFOHEADER bitmapHeader{};
	bitmapHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapHeader.biWidth = width;
	// 高さを負にすると、先頭の画素を上端として扱うTop-Down BMPになります。
	bitmapHeader.biHeight = -height;
	bitmapHeader.biPlanes = 1;
	bitmapHeader.biBitCount = 32;
	bitmapHeader.biCompression = BI_RGB;

	std::ofstream file(filePath, std::ios::binary);
	if (!file) {
		return false;
	}
	file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
	file.write(reinterpret_cast<const char*>(&bitmapHeader), sizeof(bitmapHeader));
	file.write(reinterpret_cast<const char*>(pixels.data()), imageSize);
	return file.good();
}

bool GameViewCapture::BeginRecordingAvi(const std::filesystem::path& filePath, int width, int height, int frameRate)
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
	PAVIFILE aviFile = nullptr;
	HRESULT result = AVIFileOpenW(&aviFile, filePath.wstring().c_str(), OF_WRITE | OF_CREATE, nullptr);
	if (FAILED(result)) {
		AVIFileExit();
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
		AVIFileExit();
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
		AVIFileExit();
		return false;
	}

	recordingAviFile_ = aviFile;
	recordingAviStream_ = aviStream;
	recordingVideoPath_ = filePath;
	recordingVideoWidth_ = width;
	recordingVideoHeight_ = height;
	return true;
}

bool GameViewCapture::AppendRecordingFrame(const std::vector<unsigned char>& pixels, int width, int height)
{
	if (!recordingAviStream_ ||
		pixels.empty() ||
		width != recordingVideoWidth_ ||
		height != recordingVideoHeight_) {
		return false;
	}

	// RenderTextureは上から下、AVIは下から上の行順を期待するため、行を反転して渡します。
	const size_t rowSize = static_cast<size_t>(width) * 4;
	std::vector<unsigned char> bottomUpPixels(pixels.size());
	for (int y = 0; y < height; ++y) {
		const unsigned char* source = pixels.data() + static_cast<size_t>(height - 1 - y) * rowSize;
		unsigned char* destination = bottomUpPixels.data() + static_cast<size_t>(y) * rowSize;
		std::memcpy(destination, source, rowSize);
	}

	const HRESULT result = AVIStreamWrite(
		static_cast<PAVISTREAM>(recordingAviStream_),
		recordingFrameIndex_,
		1,
		bottomUpPixels.data(),
		static_cast<LONG>(bottomUpPixels.size()),
		AVIIF_KEYFRAME,
		nullptr,
		nullptr);
	return SUCCEEDED(result);
}

void GameViewCapture::EndRecordingAvi()
{
	if (recordingAviStream_) {
		AVIStreamRelease(static_cast<PAVISTREAM>(recordingAviStream_));
		recordingAviStream_ = nullptr;
	}
	if (recordingAviFile_) {
		AVIFileRelease(static_cast<PAVIFILE>(recordingAviFile_));
		recordingAviFile_ = nullptr;
		AVIFileExit();
	}
	recordingVideoWidth_ = 0;
	recordingVideoHeight_ = 0;
}

bool GameViewCapture::SaveSingleFrameAvi(
	const std::filesystem::path& filePath,
	const std::vector<unsigned char>& pixels,
	int width,
	int height)
{
	recordingFrameIndex_ = 0;
	if (!BeginRecordingAvi(filePath, width, height)) {
		return false;
	}
	const bool success = AppendRecordingFrame(pixels, width, height);
	EndRecordingAvi();
	if (success) {
		lastVideoPath_ = filePath;
	}
	return success;
}

void GameViewCapture::UpdateRecording()
{
	if (!isRecording_) {
		return;
	}

	const float deltaTime = DirectXCommon::GetInstance()->GetDeltaTime();
	recordingTime_ += deltaTime;
	recordingFrameTimer_ += deltaTime;

	constexpr float captureInterval = 1.0f / 10.0f;
	if (recordingFrameIndex_ > 0 && recordingFrameTimer_ < captureInterval) {
		return;
	}
	while (recordingFrameTimer_ >= captureInterval) {
		recordingFrameTimer_ -= captureInterval;
	}

	std::vector<unsigned char> pixels;
	int width = 0;
	int height = 0;
	if (!CapturePixels(pixels, width, height) || !AppendRecordingFrame(pixels, width, height)) {
		isRecording_ = false;
		EndRecordingAvi();
		lastMessage_ = "Recording stopped because AVI frame capture failed.";
		return;
	}

	++recordingFrameIndex_;
}

void GameViewCapture::UpdateReplayBuffer()
{
	if (!replayBufferEnabled_ || isRecording_) {
		return;
	}

	replayFrameTimer_ += DirectXCommon::GetInstance()->GetDeltaTime();
	constexpr float captureInterval = 1.0f / static_cast<float>(kReplayFramesPerSecond_);
	if (replayFrameTimer_ < captureInterval) {
		return;
	}
	while (replayFrameTimer_ >= captureInterval) {
		replayFrameTimer_ -= captureInterval;
	}

	std::vector<unsigned char> sourcePixels;
	int sourceWidth = 0;
	int sourceHeight = 0;
	if (!CapturePixels(sourcePixels, sourceWidth, sourceHeight) || sourcePixels.empty() || sourceWidth <= 0 || sourceHeight <= 0) {
		return;
	}

	const float scale = (std::min)(
		1.0f,
		(std::min)(
			static_cast<float>(kReplayWidth_) / static_cast<float>(sourceWidth),
			static_cast<float>(kReplayHeight_) / static_cast<float>(sourceHeight)));
	const int targetWidth = (std::max)(1, static_cast<int>(static_cast<float>(sourceWidth) * scale));
	const int targetHeight = (std::max)(1, static_cast<int>(static_cast<float>(sourceHeight) * scale));

	if (!replayFrames_.empty() &&
		(replayFrames_.front().width != targetWidth || replayFrames_.front().height != targetHeight)) {
		replayFrames_.clear();
	}

	ReplayFrame replayFrame{};
	replayFrame.width = targetWidth;
	replayFrame.height = targetHeight;
	replayFrame.pixels.resize(static_cast<size_t>(targetWidth) * static_cast<size_t>(targetHeight) * 4);
	for (int y = 0; y < targetHeight; ++y) {
		const int sourceY = (std::min)(sourceHeight - 1, y * sourceHeight / targetHeight);
		for (int x = 0; x < targetWidth; ++x) {
			const int sourceX = (std::min)(sourceWidth - 1, x * sourceWidth / targetWidth);
			const size_t sourceOffset =
				(static_cast<size_t>(sourceY) * static_cast<size_t>(sourceWidth) + static_cast<size_t>(sourceX)) * 4;
			const size_t targetOffset =
				(static_cast<size_t>(y) * static_cast<size_t>(targetWidth) + static_cast<size_t>(x)) * 4;
			std::memcpy(replayFrame.pixels.data() + targetOffset, sourcePixels.data() + sourceOffset, 4);
		}
	}

	replayFrames_.push_back(std::move(replayFrame));
	const size_t maximumFrameCount = static_cast<size_t>(kReplaySeconds_ * kReplayFramesPerSecond_);
	while (replayFrames_.size() > maximumFrameCount) {
		replayFrames_.pop_front();
	}
}

bool GameViewCapture::SaveReplayClip()
{
	if (replayFrames_.empty() || isRecording_ || !CreateDirectories()) {
		return false;
	}

	const std::filesystem::path replayPath =
		GetCaptureDirectory("Replays") / ("CG2_replay_" + MakeTimestampString() + ".avi");
	const ReplayFrame& firstFrame = replayFrames_.front();
	recordingFrameIndex_ = 0;
	if (!BeginRecordingAvi(replayPath, firstFrame.width, firstFrame.height, kReplayFramesPerSecond_)) {
		return false;
	}

	bool success = true;
	for (const ReplayFrame& frame : replayFrames_) {
		if (frame.width != firstFrame.width || frame.height != firstFrame.height ||
			!AppendRecordingFrame(frame.pixels, frame.width, frame.height)) {
			success = false;
			break;
		}
		++recordingFrameIndex_;
	}
	EndRecordingAvi();
	if (!success) {
		return false;
	}

	lastReplayPath_ = replayPath;
	lastVideoPath_ = replayPath;
	return true;
}

void GameViewCapture::DrawImGui()
{
#ifdef USE_IMGUI
	const std::filesystem::path screenshotDirectory = GetCaptureDirectory("Screenshots");
	const std::filesystem::path videoDirectory = GetCaptureDirectory("Videos");
	const std::filesystem::path replayDirectory = GetCaptureDirectory("Replays");
	if (!CreateDirectories()) {
		lastMessage_ = "Could not create capture folders under resources/Captures.";
	}

	ImGui::TextUnformatted("Capture");
	ImGui::SameLine();
	ImGui::TextDisabled("Game View only");

	if (ImGui::Button("Photo")) {
		const std::filesystem::path capturePath = screenshotDirectory / ("CG2_" + MakeTimestampString() + ".bmp");
		std::vector<unsigned char> pixels;
		int width = 0;
		int height = 0;
		if (!CapturePixels(pixels, width, height)) {
			lastMessage_ = "Screenshot failed: Game View pixels could not be captured.";
		} else if (!SavePixelsAsBmp(capturePath, pixels, width, height)) {
			lastMessage_ = "Screenshot failed: could not write BMP to " + capturePath.string();
		} else {
			lastScreenshotPath_ = capturePath;
			lastMessage_ = "Saved screenshot: " + capturePath.string();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button(isRecording_ ? "Stop" : "Record")) {
		if (isRecording_) {
			isRecording_ = false;
			EndRecordingAvi();
			lastVideoPath_ = recordingVideoPath_;
			lastMessage_ = "Saved video: " + recordingVideoPath_.string();
		} else {
			recordingVideoPath_ = videoDirectory / ("CG2_" + MakeTimestampString() + ".avi");
			std::vector<unsigned char> pixels;
			int width = 0;
			int height = 0;
			if (!CapturePixels(pixels, width, height)) {
				lastMessage_ = "Recording failed: Game View pixels could not be captured.";
			} else if (BeginRecordingAvi(recordingVideoPath_, width, height)) {
				isRecording_ = true;
				recordingTime_ = 0.0f;
				recordingFrameTimer_ = 0.0f;
				recordingFrameIndex_ = 0;
				if (AppendRecordingFrame(pixels, width, height)) {
					++recordingFrameIndex_;
					lastMessage_ = "Recording video: " + recordingVideoPath_.string();
				} else {
					EndRecordingAvi();
					isRecording_ = false;
					lastMessage_ = "Recording failed to write first frame.";
				}
			} else {
				lastMessage_ = "Recording failed: could not create AVI at " + recordingVideoPath_.string();
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("open Photos File")) {
		lastMessage_ = OpenFolderInExplorer(screenshotDirectory)
			? "Opened screenshot folder: " + screenshotDirectory.string()
			: "Could not open screenshot folder: " + screenshotDirectory.string();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("open Videos File")) {
		lastMessage_ = OpenFolderInExplorer(videoDirectory)
			? "Opened video folder: " + videoDirectory.string()
			: "Could not open video folder: " + videoDirectory.string();
	}

	if (ImGui::Checkbox("Keep 30s Replay", &replayBufferEnabled_) && !replayBufferEnabled_) {
		replayFrames_.clear();
		replayFrameTimer_ = 0.0f;
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(isRecording_ || !replayBufferEnabled_ || replayFrames_.empty());
	if (ImGui::Button("Save Last 30s")) {
		lastMessage_ = SaveReplayClip()
			? "Saved replay: " + lastReplayPath_.string()
			: "Replay save failed.";
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::SmallButton("open Replays File")) {
		lastMessage_ = OpenFolderInExplorer(replayDirectory)
			? "Opened replay folder: " + replayDirectory.string()
			: "Could not open replay folder: " + replayDirectory.string();
	}
	const float replaySeconds = static_cast<float>(replayFrames_.size()) / static_cast<float>(kReplayFramesPerSecond_);
	size_t replayBytes = 0;
	for (const ReplayFrame& frame : replayFrames_) {
		replayBytes += frame.pixels.size();
	}
	ImGui::TextDisabled(
		"Replay buffer: %.1f / %d sec (480x270, 5 FPS, %.1f MB)",
		replaySeconds,
		kReplaySeconds_,
		static_cast<double>(replayBytes) / (1024.0 * 1024.0));

	if (isRecording_) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "REC %.2fs  %d frames", recordingTime_, recordingFrameIndex_);
	}
	if (!lastMessage_.empty()) {
		ImGui::TextWrapped("%s", lastMessage_.c_str());
	}
#endif
}
