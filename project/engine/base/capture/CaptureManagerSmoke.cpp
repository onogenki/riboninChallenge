#include "CaptureManager.h"

#include "SceneManager.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <sstream>

// キャプチャ機能の自動確認だけをまとめる実装ファイル。
// 通常の撮影・録画処理と分けることで、ゲーム中に必要な処理を追いやすくする。
bool CaptureManager::IsVisiblyNonBlack(
	const std::vector<unsigned char>& pixels,
	int width,
	int height) const
{
	const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
	if (width <= 0 || height <= 0 || pixels.size() < pixelCount * 4) {
		return false;
	}

	size_t visiblePixels = 0;
	for (size_t index = 0; index < pixelCount; ++index) {
		const size_t offset = index * 4;
		const unsigned int brightness =
			static_cast<unsigned int>(pixels[offset + 0]) +
			static_cast<unsigned int>(pixels[offset + 1]) +
			static_cast<unsigned int>(pixels[offset + 2]);
		if (brightness >= 24) {
			++visiblePixels;
		}
	}
	return visiblePixels >= pixelCount / 100;
}

void CaptureManager::UpdateSmokeBeforeCapture()
{
	if (!smokeEnabled_ || smokeFinished_) {
		return;
	}

	++smokeFrame_;
	const std::string& sceneName = SceneManager::GetInstance()->GetCurrentSceneName();
	if (sceneName == "TITLE") {
		if (smokeFrame_ == 5) {
			++recordingGeneration_;
			recordingStartRequested_ = true;
		}
		if (smokeFrame_ >= 6 && smokeFrame_ <= 13) {
			QueuePhotoCapture();
		}
		if (smokeFrame_ == 20 && !SceneManager::GetInstance()->ChangeScene("DEBUG")) {
			FinishSmoke(false, "Could not request TITLE to DEBUG scene change.");
		}
		return;
	}

	if (sceneName == "STAGE1") {
		++smokeDebugFrame_;
		// 開始演出中・Player移動後・通常Cameraへの引継ぎ後をそれぞれ保存します。
		const bool captureInitialState =
			smokeDebugFrame_ >= 5 && smokeDebugFrame_ <= 8;
		const bool captureMovedState =
			smokeDebugFrame_ >= 60 && smokeDebugFrame_ <= 63;
		// StageStartのCamera移動が終わった後も撮影し、開始位置の配置物を確認します。
		const bool captureGameplayState =
			smokeDebugFrame_ >= 300 && smokeDebugFrame_ <= 303;
		if (captureInitialState || captureMovedState || captureGameplayState) {
			QueuePhotoCapture();
		}
		if (smokeDebugFrame_ == 340) {
			std::error_code errorCode;
			bool filesExist = !smokePhotoPaths_.empty();
			for (const std::filesystem::path& path : smokePhotoPaths_) {
				filesExist = filesExist &&
					path.extension() == ".png" &&
					std::filesystem::exists(path, errorCode) &&
					!errorCode;
				errorCode.clear();
			}
			const bool success =
				filesExist &&
				smokeSavedPhotos_ >= 1 &&
				smokeCaptureAttempts_ >= 1 &&
				smokeVisibleCaptures_ == smokeCaptureAttempts_;
			std::ostringstream message;
			message << "scene=STAGE1 photos=" << smokeSavedPhotos_
				<< " visible=" << smokeVisibleCaptures_ << '/' << smokeCaptureAttempts_;
			FinishSmoke(success, message.str());
		}
		return;
	}

	if (sceneName == "DEBUG") {
		++smokeDebugFrame_;
		if (smokeDebugFrame_ <= 8) {
			QueuePhotoCapture();
		}
		if (smokeDebugFrame_ == 45 && isRecording_) {
			smokeVideoFrameCount_ = recordingFrameIndex_;
			StopRecording("Capture smoke stopped the cross-scene video.");
			smokeMainVideoPath_ = lastVideoPath_;
		}
		if (smokeDebugFrame_ == 48) {
			if (!SaveReplayClip()) {
				FinishSmoke(false, "Could not save the shared replay buffer.");
				return;
			}
		}
		if (smokeDebugFrame_ == 55) {
			WaitForReplaySave();
			smokeReplayPath_ = lastReplayPath_;
			std::error_code errorCode;
			bool filesExist = smokePhotoPaths_.size() == 16;
			for (const std::filesystem::path& path : smokePhotoPaths_) {
				filesExist = filesExist &&
					path.extension() == ".png" &&
					std::filesystem::exists(path, errorCode) &&
					!errorCode;
				errorCode.clear();
			}
			const bool videoExists =
				!smokeMainVideoPath_.empty() &&
				std::filesystem::exists(smokeMainVideoPath_, errorCode) &&
				!errorCode &&
				std::filesystem::file_size(smokeMainVideoPath_, errorCode) > 1024 &&
				!errorCode;
			errorCode.clear();
			const bool replayExists =
				!smokeReplayPath_.empty() &&
				std::filesystem::exists(smokeReplayPath_, errorCode) &&
				!errorCode &&
				std::filesystem::file_size(smokeReplayPath_, errorCode) > 1024 &&
				!errorCode;
			bool mp4ContentValid = true;
			double directError = 0.0;
			double flippedError = 0.0;
			double swappedError = 0.0;
			if (videoExists && smokeMainVideoPath_.extension() == ".mp4") {
				std::vector<unsigned char> decodedFirst;
				std::vector<unsigned char> decodedLast;
				int decodedWidth = 0;
				int decodedHeight = 0;
				mp4ContentValid = DecodeMp4FirstAndLastFrame(
					smokeMainVideoPath_, decodedFirst, decodedLast, decodedWidth, decodedHeight);
				if (mp4ContentValid) {
					const bool dimensionsMatch =
						decodedWidth == smokeVideoWidth_ && decodedHeight == smokeVideoHeight_;
					directError = (
						CalculateFrameError(smokeFirstVideoFrame_, decodedFirst, decodedWidth, decodedHeight, false, false) +
						CalculateFrameError(smokeLastVideoFrame_, decodedLast, decodedWidth, decodedHeight, false, false)) * 0.5;
					flippedError = (
						CalculateFrameError(smokeFirstVideoFrame_, decodedFirst, decodedWidth, decodedHeight, true, false) +
						CalculateFrameError(smokeLastVideoFrame_, decodedLast, decodedWidth, decodedHeight, true, false)) * 0.5;
					swappedError = (
						CalculateFrameError(smokeFirstVideoFrame_, decodedFirst, decodedWidth, decodedHeight, false, true) +
						CalculateFrameError(smokeLastVideoFrame_, decodedLast, decodedWidth, decodedHeight, false, true)) * 0.5;
					mp4ContentValid =
						dimensionsMatch &&
						directError < 55.0 &&
						directError < flippedError * 0.85 &&
						directError < swappedError * 0.95;

					const std::filesystem::path decodedFirstPath =
						GetCaptureDirectory("Screenshots") / "CG2_mp4_decoded_first.bmp";
					const std::filesystem::path decodedLastPath =
						GetCaptureDirectory("Screenshots") / "CG2_mp4_decoded_last.bmp";
					mp4ContentValid = mp4ContentValid &&
						SavePixelsAsBmp(decodedFirstPath, decodedFirst, decodedWidth, decodedHeight) &&
						SavePixelsAsBmp(decodedLastPath, decodedLast, decodedWidth, decodedHeight);
				}
			}
			bool replayContentValid = true;
			double replayDirectError = 0.0;
			double replayFlippedError = 0.0;
			double replaySwappedError = 0.0;
			int decodedReplayFrameCount = 0;
			if (replayExists && smokeReplayPath_.extension() == ".mp4") {
				std::vector<unsigned char> decodedReplayFirst;
				std::vector<unsigned char> decodedReplayLast;
				int decodedReplayWidth = 0;
				int decodedReplayHeight = 0;
				replayContentValid = DecodeMp4FirstAndLastFrame(
					smokeReplayPath_,
					decodedReplayFirst,
					decodedReplayLast,
					decodedReplayWidth,
					decodedReplayHeight,
					&decodedReplayFrameCount);
				if (replayContentValid) {
					replayDirectError = (
						CalculateFrameError(smokeFirstReplayFrame_, decodedReplayFirst, decodedReplayWidth, decodedReplayHeight, false, false) +
						CalculateFrameError(smokeLastReplayFrame_, decodedReplayLast, decodedReplayWidth, decodedReplayHeight, false, false)) * 0.5;
					replayFlippedError = (
						CalculateFrameError(smokeFirstReplayFrame_, decodedReplayFirst, decodedReplayWidth, decodedReplayHeight, true, false) +
						CalculateFrameError(smokeLastReplayFrame_, decodedReplayLast, decodedReplayWidth, decodedReplayHeight, true, false)) * 0.5;
					replaySwappedError = (
						CalculateFrameError(smokeFirstReplayFrame_, decodedReplayFirst, decodedReplayWidth, decodedReplayHeight, false, true) +
						CalculateFrameError(smokeLastReplayFrame_, decodedReplayLast, decodedReplayWidth, decodedReplayHeight, false, true)) * 0.5;
					replayContentValid =
						decodedReplayFrameCount == smokeReplayFrameCount_ &&
						smokeReplayFrameCount_ >= 20 &&
						replayDirectError < 55.0 &&
						replayDirectError < replayFlippedError * 0.85 &&
						replayDirectError < replaySwappedError * 0.95;

					const std::filesystem::path replayFirstPath =
						GetCaptureDirectory("Screenshots") / "CG2_replay_decoded_first.bmp";
					const std::filesystem::path replayLastPath =
						GetCaptureDirectory("Screenshots") / "CG2_replay_decoded_last.bmp";
					replayContentValid = replayContentValid &&
						SavePixelsAsBmp(replayFirstPath, decodedReplayFirst, decodedReplayWidth, decodedReplayHeight) &&
						SavePixelsAsBmp(replayLastPath, decodedReplayLast, decodedReplayWidth, decodedReplayHeight);
				}
			}
			const float averageFps = smokePerformanceTime_ > 0.0f
				? static_cast<float>(smokePerformanceFrameCount_) / smokePerformanceTime_
				: 0.0f;
			const bool success =
				filesExist &&
				videoExists &&
				replayExists &&
				mp4ContentValid &&
				replayContentValid &&
				smokeReplayDroppedFrames_ == 0 &&
				averageFps >= 30.0f &&
				smokeSavedPhotos_ == 16 &&
				smokeCaptureAttempts_ == 16 &&
				smokeVisibleCaptures_ == smokeCaptureAttempts_ &&
				smokeVideoFrameCount_ >= 2;
			std::ostringstream message;
			message << "scene=DEBUG photos=" << smokeSavedPhotos_
				<< " visible=" << smokeVisibleCaptures_ << '/' << smokeCaptureAttempts_
				<< " videoFrames=" << smokeVideoFrameCount_
				<< " directError=" << directError
				<< " flippedError=" << flippedError
				<< " swappedError=" << swappedError
				<< " replayFrames=" << smokeReplayFrameCount_ << '/' << decodedReplayFrameCount
				<< " replayDropped=" << smokeReplayDroppedFrames_
				<< " replayDirectError=" << replayDirectError
				<< " replayFlippedError=" << replayFlippedError
				<< " replaySwappedError=" << replaySwappedError
				<< " averageFps=" << averageFps
				<< " maxFrameMs=" << smokeMaximumDeltaTime_ * 1000.0f
				<< " readbackMs=" << (smokeReadbackSamples_ > 0 ? smokeReadbackMilliseconds_ / smokeReadbackSamples_ : 0.0)
				<< " videoQueueMs=" << (smokeVideoSamples_ > 0 ? smokeVideoMilliseconds_ / smokeVideoSamples_ : 0.0)
				<< " videoWorkerMs=" << (smokeVideoWorkerSamples_ > 0 ? smokeVideoWorkerMilliseconds_ / smokeVideoWorkerSamples_ : 0.0)
				<< " replayMs=" << (smokeReplaySamples_ > 0 ? smokeReplayMilliseconds_ / smokeReplaySamples_ : 0.0)
				<< " video=" << smokeMainVideoPath_.string()
				<< " replay=" << smokeReplayPath_.string();
			FinishSmoke(success, message.str());
		}
	}
}

void CaptureManager::UpdateSmokeAfterCapture(bool captured, bool visiblyNonBlack)
{
	if (!smokeEnabled_ || smokeFinished_) {
		return;
	}
	++smokeCaptureAttempts_;
	if (captured && visiblyNonBlack) {
		++smokeVisibleCaptures_;
	}
}

void CaptureManager::FinishSmoke(bool success, const std::string& message)
{
	if (smokeFinished_) {
		return;
	}
	smokeFinished_ = true;
	std::ofstream log(smokeLogPath_, std::ios::app);
	if (log) {
		log << (success ? "SUCCESS: " : "FAILURE: ") << message << '\n';
	}
	PostQuitMessage(success ? 0 : 1);
}
