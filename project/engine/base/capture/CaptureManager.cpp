#include "CaptureManager.h"

#include "DirectXCommon.h"
#include "PostEffect.h"
#include "SceneManager.h"

#include <Windows.h>
#include <wincodec.h>
#include <shellapi.h>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
std::string GetEnvironmentString(const char* name)
{
	char* value = nullptr;
	size_t size = 0;
	if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) {
		return {};
	}
	std::string result(value);
	std::free(value);
	return result;
}

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

CaptureManager* CaptureManager::GetInstance()
{
	static CaptureManager instance;
	return &instance;
}

// 撮影機能を初期化し、対応しない構成のSmokeは待ち続けず失敗を記録します。
void CaptureManager::Initialize()
{
	if (initialized_) {
		return;
	}
	initialized_ = true;
	photoRequestCount_ = 0;
	photoCapturePending_ = false;
	recordingStartRequested_ = false;
	recordingStartCapturePending_ = false;
	recordingGeneration_ = 0;
	pendingCaptureRequests_.clear();
	droppedCaptureFrames_ = 0;
	CreateCaptureDirectories();
	smokeEnabled_ = GetEnvironmentString("CG2_CAPTURE_SMOKE") == "1";
	performanceBaselineEnabled_ = GetEnvironmentString("CG2_CAPTURE_PERF_BASELINE") == "1";
	if (performanceBaselineEnabled_) {
		replayBufferEnabled_ = false;
	}
	smokeFinished_ = false;
	smokeFrame_ = 0;
	smokeDebugFrame_ = 0;
	smokeCaptureAttempts_ = 0;
	smokeVisibleCaptures_ = 0;
	smokeSavedPhotos_ = 0;
	smokeVideoFrameCount_ = 0;
	smokePhotoPaths_.clear();
	smokeFirstVideoFrame_.clear();
	smokeLastVideoFrame_.clear();
	smokeFirstReplayFrame_.clear();
	smokeLastReplayFrame_.clear();
	smokeVideoWidth_ = 0;
	smokeVideoHeight_ = 0;
	smokeReplayFrameCount_ = 0;
	smokeReplayDroppedFrames_ = 0;
	smokePerformanceFrameCount_ = 0;
	smokePerformanceTime_ = 0.0f;
	smokeMaximumDeltaTime_ = 0.0f;
	smokeReadbackMilliseconds_ = 0.0;
	smokeVideoMilliseconds_ = 0.0;
	smokeReplayMilliseconds_ = 0.0;
	smokeVideoWorkerMilliseconds_ = 0.0;
	smokeReadbackSamples_ = 0;
	smokeVideoSamples_ = 0;
	smokeReplaySamples_ = 0;
	smokeVideoWorkerSamples_ = 0;
	smokeMainVideoPath_.clear();
	smokeReplayPath_.clear();
	smokeTrackActiveVideo_ = false;
	smokeLogPath_ = std::filesystem::path("logs") / "capture_manager_smoke.log";
	const std::string requestedFormat = GetEnvironmentString("CG2_CAPTURE_VIDEO_FORMAT");
	if (requestedFormat == "AVI" || requestedFormat == "avi") {
		selectedVideoFormat_ = VideoFormat::Avi;
	} else if (requestedFormat == "MP4" || requestedFormat == "mp4") {
		selectedVideoFormat_ = VideoFormat::Mp4;
	}
	if (smokeEnabled_) {
		std::ofstream log(smokeLogPath_, std::ios::trunc);
		if (log) {
			log << "START common capture smoke\n";
		}
	}
#ifndef USE_IMGUI
	// Releaseには撮影更新が無いため、Smokeを開始したまま無限待機させません。
	if (smokeEnabled_) {
		FinishSmoke(false, "Capture smoke requires a Development or Debug build with USE_IMGUI.");
	}
#endif
#ifdef USE_IMGUI
	StartVideoEncoder();
	StartReplayEncoder();
#endif
}

void CaptureManager::Finalize()
{
	if (!initialized_) {
		return;
	}
	WaitForReplaySave();
	ProcessCompletedCaptures(true);
	if (isRecording_) {
		StopRecording("Recording stopped when the game closed.");
	} else {
		EndActiveVideo();
	}
	StopVideoEncoder();
	StopReplayEncoder();
	ClearReplayBuffer();
	photoRequestCount_ = 0;
	photoCapturePending_ = false;
	recordingStartRequested_ = false;
	recordingStartCapturePending_ = false;
	pendingCaptureRequests_.clear();
	initialized_ = false;
}

void CaptureManager::QueuePhotoCapture()
{
	++photoRequestCount_;
}

std::filesystem::path CaptureManager::GetCaptureDirectory(const char* folderName) const
{
	std::error_code errorCode;
	const std::filesystem::path resourceDirectory = std::filesystem::absolute("resources", errorCode);
	return (errorCode ? std::filesystem::path("resources") : resourceDirectory) / "Captures" / folderName;
}

bool CaptureManager::CreateCaptureDirectories() const
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

std::string CaptureManager::MakeTimestampString() const
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

bool CaptureManager::CapturePixels(std::vector<unsigned char>& pixels, int& width, int& height) const
{
	return DirectXCommon::GetInstance()->CaptureGameTexturePixels(
		pixels,
		width,
		height,
		PostEffect::GetInstance()->IsEnabled());
}

bool CaptureManager::SavePixelsAsBmp(
	const std::filesystem::path& filePath,
	const std::vector<unsigned char>& pixels,
	int width,
	int height) const
{
	if (pixels.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4 || width <= 0 || height <= 0) {
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

bool CaptureManager::SavePixelsAsPng(
	const std::filesystem::path& filePath,
	const std::vector<unsigned char>& pixels,
	int width,
	int height) const
{
	if (width <= 0 || height <= 0 ||
		pixels.size() < static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
		return false;
	}

	std::error_code errorCode;
	std::filesystem::create_directories(filePath.parent_path(), errorCode);
	if (errorCode) {
		return false;
	}
	std::vector<unsigned char> opaquePixels = pixels;
	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
	for (size_t index = 0; index < pixelCount; ++index) {
		opaquePixels[index * 4 + 3] = 255;
	}

	DirectX::Image image{};
	image.width = static_cast<size_t>(width);
	image.height = static_cast<size_t>(height);
	image.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	image.rowPitch = static_cast<size_t>(width) * 4;
	image.slicePitch = image.rowPitch * static_cast<size_t>(height);
	image.pixels = opaquePixels.data();
	return SUCCEEDED(DirectX::SaveToWICFile(
		image,
		DirectX::WIC_FLAGS_FORCE_SRGB,
		GUID_ContainerFormatPng,
		filePath.wstring().c_str(),
		&GUID_WICPixelFormat32bppBGRA));
}

void CaptureManager::ProcessCompletedCaptures(bool waitForGpu)
{
	if (pendingCaptureRequests_.empty()) {
		return;
	}
	if (waitForGpu) {
		DirectXCommon::GetInstance()->WaitForGPU();
	}

	while (!pendingCaptureRequests_.empty()) {
		std::vector<unsigned char> pixels;
		int width = 0;
		int height = 0;
		const auto readbackStart = std::chrono::steady_clock::now();
		const bool captureReady =
			DirectXCommon::GetInstance()->TryGetGameTextureCapturePixels(pixels, width, height);
		if (!captureReady) {
			break;
		}
		if (smokeEnabled_) {
			smokeReadbackMilliseconds_ += std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - readbackStart).count();
			++smokeReadbackSamples_;
		}
		const PendingCaptureRequest request = pendingCaptureRequests_.front();
		pendingCaptureRequests_.pop_front();
		ProcessCapturedFrame(request, std::move(pixels), width, height);
	}
}

void CaptureManager::ProcessCapturedFrame(
	const PendingCaptureRequest& request,
	std::vector<unsigned char> pixels,
	int width,
	int height)
{
	const bool validPixels =
		width > 0 && height > 0 &&
		pixels.size() >= static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
	if (request.photo) {
		photoCapturePending_ = false;
		if (photoRequestCount_ > 0) {
			--photoRequestCount_;
		}
		const bool visiblyNonBlack = validPixels && IsVisiblyNonBlack(pixels, width, height);
		UpdateSmokeAfterCapture(validPixels, visiblyNonBlack);
		if (validPixels) {
			const std::filesystem::path capturePath =
				GetCaptureDirectory("Screenshots") / ("CG2_" + MakeTimestampString() + ".png");
			if (SavePixelsAsPng(capturePath, pixels, width, height)) {
				lastScreenshotPath_ = capturePath;
				if (smokeEnabled_) {
					++smokeSavedPhotos_;
					smokePhotoPaths_.push_back(capturePath);
				}
				lastMessage_ = "Saved screenshot: " + capturePath.string();
			} else {
				lastMessage_ = "Screenshot failed: could not write PNG to " + capturePath.string();
			}
		} else {
			lastMessage_ = "Screenshot failed: Game View pixels could not be captured.";
		}
	}
	std::shared_ptr<const std::vector<unsigned char>> sharedPixels;
	if (validPixels && (request.startRecording || request.recordingFrame || request.replayFrame)) {
		sharedPixels = std::make_shared<std::vector<unsigned char>>(std::move(pixels));
	}
	if (request.replayFrame && replayBufferEnabled_ && validPixels) {
		const auto replayStart = std::chrono::steady_clock::now();
		UpdateReplayBuffer(sharedPixels, width, height);
		if (smokeEnabled_) {
			smokeReplayMilliseconds_ += std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - replayStart).count();
			++smokeReplaySamples_;
		}
	}

	if (request.startRecording) {
		recordingStartCapturePending_ = false;
		const bool requestStillCurrent =
			recordingStartRequested_ && request.recordingGeneration == recordingGeneration_;
		recordingStartRequested_ = false;
		if (requestStillCurrent) {
			videoEncodeFailed_.store(false);
			recordingFrameIndex_ = 0;
			const int selectedFrameRate = selectedVideoFormat_ == VideoFormat::Mp4
				? kMp4RecordingFramesPerSecond_
				: kAviRecordingFramesPerSecond_;
			const auto videoStart = std::chrono::steady_clock::now();
			const bool videoStarted = validPixels && BeginSelectedVideo(
				GetCaptureDirectory("Videos"),
				"CG2_",
				width,
				height,
				selectedFrameRate) && AppendActiveVideoFrame(*sharedPixels, width, height, 0);
			if (smokeEnabled_) {
				smokeVideoMilliseconds_ += std::chrono::duration<double, std::milli>(
					std::chrono::steady_clock::now() - videoStart).count();
				++smokeVideoSamples_;
			}
			if (videoStarted) {
				isRecording_ = true;
				recordingTime_ = 0.0f;
				recordingFrameTimer_ = 0.0f;
				++recordingFrameIndex_;
				lastMessage_ = "Recording video: " + recordingVideoPath_.string();
			} else {
				StopRecording("Recording failed: MP4 and AVI could not be started.");
			}
		}
	} else if (request.recordingFrame &&
		isRecording_ && request.recordingGeneration == recordingGeneration_) {
		const auto videoStart = std::chrono::steady_clock::now();
		const bool frameQueued = validPixels && QueueVideoFrame(
			sharedPixels, width, height, recordingFrameIndex_);
		if (smokeEnabled_) {
			smokeVideoMilliseconds_ += std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - videoStart).count();
			++smokeVideoSamples_;
		}
		if (frameQueued) {
			++recordingFrameIndex_;
		} else if (videoEncodeFailed_.load() || !validPixels) {
			StopRecording("Recording stopped because the video frame could not be written.");
		} else {
			++droppedCaptureFrames_;
			++recordingFrameIndex_;
		}
	}
}

void CaptureManager::StopRecording(const std::string& message)
{
	++recordingGeneration_;
	isRecording_ = false;
	recordingStartRequested_ = false;
	recordingStartCapturePending_ = false;
	WaitForVideoEncoder();
	EndActiveVideo();
	videoEncodeFailed_.store(false);
	lastVideoPath_ = recordingVideoPath_;
	lastMessage_ = message;
}

void CaptureManager::DrawImGui()
{
#ifdef USE_IMGUI
	const std::filesystem::path screenshotDirectory = GetCaptureDirectory("Screenshots");
	const std::filesystem::path videoDirectory = GetCaptureDirectory("Videos");
	const std::filesystem::path replayDirectory = GetCaptureDirectory("Replays");
	const bool replaySaving = replaySaveInProgress_.load();
	if (!CreateCaptureDirectories()) {
		lastMessage_ = "Could not create capture folders under resources/Captures.";
	}

	ImGui::TextUnformatted("Capture");
	ImGui::SameLine();
	ImGui::TextDisabled("PNG / MP4 / All scenes");
	const int totalSeconds = static_cast<int>(recordingTime_);
	const int minutes = totalSeconds / 60;
	const float seconds = recordingTime_ - static_cast<float>(minutes * 60);
	char recordingLabel[48];
	snprintf(
		recordingLabel,
		sizeof(recordingLabel),
		(isRecording_ || recordingStartRequested_) ? "REC %02d:%04.1f" : "Video %02d:%04.1f",
		minutes,
		seconds);
	const float recordingLabelWidth = ImGui::CalcTextSize(recordingLabel).x;
	ImGui::SameLine();
	ImGui::SetCursorPosX((std::max)(
		ImGui::GetCursorPosX(),
		ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - recordingLabelWidth - 180.0f));
	ImGui::BeginDisabled(replaySaving);
	if (isRecording_ || recordingStartRequested_) {
		ImGui::TextColored(ImVec4(1.0f, 0.12f, 0.12f, 1.0f), "%s", recordingLabel);
	} else {
		ImGui::TextDisabled("%s", recordingLabel);
	}

	ImGui::BeginDisabled(photoRequestCount_ > 0);
	if (ImGui::Button("Photo")) {
		QueuePhotoCapture();
		lastMessage_ = "Taking photo after this frame...";
	}
	ImGui::EndDisabled();
	ImGui::SameLine();

	if (isRecording_ || recordingStartRequested_) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.05f, 0.07f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.08f, 0.10f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.62f, 0.02f, 0.04f, 1.0f));
		if (ImGui::Button("Stop Video")) {
			StopRecording("Saved video: " + recordingVideoPath_.string());
		}
		ImGui::PopStyleColor(3);
	} else if (ImGui::Button("Video")) {
		++recordingGeneration_;
		recordingStartRequested_ = true;
		lastMessage_ = "Starting video after this frame...";
	}
	ImGui::EndDisabled();

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
		ClearReplayBuffer();
	}
	ImGui::SameLine();
	size_t replayFrameCount = 0;
	size_t replayBytes = 0;
	size_t queuedFrames = 0;
	size_t droppedFrames = 0;
	{
		std::lock_guard lock(replayMutex_);
		replayFrameCount = replayFrames_.size();
		queuedFrames = replayEncodeJobs_.size() + replayActiveJobs_;
		droppedFrames = replayDroppedFrames_;
		for (const ReplayFrame& frame : replayFrames_) {
			replayBytes += frame.encodedPixels.size();
		}
	}
	ImGui::BeginDisabled(
		isRecording_ || recordingStartRequested_ || replaySaving || !replayBufferEnabled_ || replayFrameCount == 0);
	if (ImGui::Button("Save Last 30s")) {
		lastMessage_ = SaveReplayClip()
			? "Saving replay in background..."
			: "Replay save could not be started.";
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::SmallButton("open Replays File")) {
		lastMessage_ = OpenFolderInExplorer(replayDirectory)
			? "Opened replay folder: " + replayDirectory.string()
			: "Could not open replay folder: " + replayDirectory.string();
	}

	const float replaySeconds = static_cast<float>(replayFrameCount) / static_cast<float>(kReplayFramesPerSecond_);
	ImGui::TextDisabled(
		"Replay: %.1f / %d sec (960x540, 30 FPS, %.1f MB, queue %zu, dropped %zu)",
		replaySeconds,
		kReplaySeconds_,
		static_cast<double>(replayBytes) / (1024.0 * 1024.0),
		queuedFrames,
		droppedFrames);
	if (replaySaving) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Saving MP4...");
	}
	if (!lastMessage_.empty()) {
		ImGui::TextWrapped("%s", lastMessage_.c_str());
	}
#endif
}

void CaptureManager::UpdateAfterDraw()
{
#ifdef USE_IMGUI
	if (!initialized_) {
		return;
	}
	if (performanceBaselineEnabled_) {
		const std::string& performanceScene = SceneManager::GetInstance()->GetCurrentSceneName();
		if (performanceScene == "DEBUG" || performanceScene == "STAGE1") {
			++smokeDebugFrame_;
			const float deltaTime = DirectXCommon::GetInstance()->GetDeltaTime();
			if (smokeDebugFrame_ > 8 && smokeDebugFrame_ < 69) {
				++smokePerformanceFrameCount_;
				smokePerformanceTime_ += deltaTime;
				smokeMaximumDeltaTime_ = (std::max)(smokeMaximumDeltaTime_, deltaTime);
			}
			if (smokeDebugFrame_ == 69) {
				const float averageFps = smokePerformanceTime_ > 0.0f
					? static_cast<float>(smokePerformanceFrameCount_) / smokePerformanceTime_
					: 0.0f;
				std::ofstream log(std::filesystem::path("logs") / "capture_performance_baseline.log", std::ios::trunc);
				if (log) {
					log << "BASELINE: scene=" << performanceScene
						<< " averageFps=" << averageFps
						<< " maxFrameMs=" << smokeMaximumDeltaTime_ * 1000.0f << '\n';
				}
				smokeFinished_ = true;
				PostQuitMessage(0);
			}
		}
		return;
	}
	FinishReplaySaveIfReady();
	if (isRecording_ && videoEncodeFailed_.load()) {
		StopRecording("Recording stopped because the MP4 encoder rejected a frame.");
	}
	ProcessCompletedCaptures();
	UpdateSmokeBeforeCapture();
	if (smokeFinished_) {
		return;
	}

	const float deltaTime = DirectXCommon::GetInstance()->GetDeltaTime();
	if (smokeEnabled_ &&
		SceneManager::GetInstance()->GetCurrentSceneName() == "DEBUG" &&
		smokeDebugFrame_ > 8 &&
		smokeDebugFrame_ < 45) {
		++smokePerformanceFrameCount_;
		smokePerformanceTime_ += deltaTime;
		smokeMaximumDeltaTime_ = (std::max)(smokeMaximumDeltaTime_, deltaTime);
	}
	bool recordingFrameDue = false;
	if (isRecording_) {
		recordingTime_ += deltaTime;
		recordingFrameTimer_ += deltaTime;
		const float interval = 1.0f / static_cast<float>(recordingFramesPerSecond_);
		if (recordingFrameTimer_ >= interval) {
			while (recordingFrameTimer_ >= interval) {
				recordingFrameTimer_ -= interval;
			}
			recordingFrameDue = true;
		}
	}

	bool replayFrameDue = false;
	if (replayBufferEnabled_) {
		replayFrameTimer_ += deltaTime;
		const float interval = 1.0f / static_cast<float>(kReplayFramesPerSecond_);
		if (replayFrameTimer_ >= interval) {
			while (replayFrameTimer_ >= interval) {
				replayFrameTimer_ -= interval;
			}
			replayFrameDue = true;
		}
	}

	PendingCaptureRequest request{};
	request.photo = photoRequestCount_ > 0 && !photoCapturePending_;
	request.startRecording = recordingStartRequested_ && !recordingStartCapturePending_;
	request.recordingFrame = recordingFrameDue && isRecording_;
	request.replayFrame = replayFrameDue && replayBufferEnabled_;
	request.recordingGeneration = recordingGeneration_;
	if (!request.photo && !request.startRecording && !request.recordingFrame && !request.replayFrame) {
		return;
	}

	if (!DirectXCommon::GetInstance()->QueueGameTextureCapture(
		PostEffect::GetInstance()->IsEnabled())) {
		if (request.recordingFrame || request.replayFrame) {
			++droppedCaptureFrames_;
		}
		return;
	}
	pendingCaptureRequests_.push_back(request);
	if (request.photo) {
		photoCapturePending_ = true;
	}
	if (request.startRecording) {
		recordingStartCapturePending_ = true;
	}
#endif
}
