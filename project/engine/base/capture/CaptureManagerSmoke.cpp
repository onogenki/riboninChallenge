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

// Sceneごとの確認だけを振り分け、通常の撮影要求を出す順番は変えません。
void CaptureManager::UpdateSmokeBeforeCapture()
{
	if (!smokeEnabled_ || smokeFinished_) {
		return;
	}

	const std::string& sceneName = Engine::SceneManager::GetInstance()->GetCurrentSceneName();
	if (sceneName == "TITLE") {
		UpdateTitleSmoke();
	} else if (sceneName == "STAGE1") {
		UpdateStageSmoke();
	} else if (sceneName == "DEBUG") {
		UpdateDebugSmoke();
	}
}

// Titleでは録画開始と写真を予約し、従来と同じ20フレーム目にDebugへ進めます。
void CaptureManager::UpdateTitleSmoke()
{
	// Loading中のフレームを数えず、Titleが実際に描かれた後から録画を開始します。
	++smokeFrame_;
	if (smokeFrame_ == 5) {
		++recordingGeneration_;
		recordingStartRequested_ = true;
	}
	if (smokeFrame_ >= 6 && smokeFrame_ <= 13) {
		QueuePhotoCapture();
	}
	if (smokeFrame_ == 20 && !Engine::SceneManager::GetInstance()->ChangeScene("DEBUG")) {
		FinishSmoke(false, "Could not request TITLE to DEBUG scene change.");
	}
}

// Stage1では開始演出・移動後・Camera引継ぎ後を別々に撮り、最後に保存結果を確認します。
void CaptureManager::UpdateStageSmoke()
{
	++smokeDebugFrame_;
	const bool captureInitialState = smokeDebugFrame_ >= 5 && smokeDebugFrame_ <= 8;
	const bool captureMovedState = smokeDebugFrame_ >= 60 && smokeDebugFrame_ <= 63;
	const bool captureGameplayState = smokeDebugFrame_ >= 300 && smokeDebugFrame_ <= 303;
	if (captureInitialState || captureMovedState || captureGameplayState) {
		QueuePhotoCapture();
	}
	if (smokeDebugFrame_ == 340) {
		VerifyStageSmoke();
	}
}

// Stage1の写真が存在し、今回撮った画像がすべて可視だったかを判定します。
void CaptureManager::VerifyStageSmoke()
{
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

// Debugでは写真・跨Scene動画・リプレイの予約と完了待ちだけを進めます。
void CaptureManager::UpdateDebugSmoke()
{
	++smokeDebugFrame_;
	if (smokeDebugFrame_ <= 8) {
		QueuePhotoCapture();
	}
	if (smokeDebugFrame_ == 45 && isRecording_) {
		smokeVideoFrameCount_ = recordingFrameIndex_;
		StopRecording("Capture smoke stopped the cross-scene video.");
		smokeMainVideoPath_ = lastVideoPath_;
	}
	if (smokeDebugFrame_ == 48 && !SaveReplayClip()) {
		FinishSmoke(false, "Could not save the shared replay buffer.");
		return;
	}
	if (smokeDebugFrame_ == 55) {
		WaitForReplaySave();
		smokeReplayPath_ = lastReplayPath_;
		VerifyDebugSmoke();
	}
}

// 元画像と復号画像を比較し、上下反転や赤青交換だけで一致した誤保存を見分けます。
CaptureManager::SmokeFrameErrors CaptureManager::CompareSmokeFrames(
	const std::vector<unsigned char>& referenceFirst,
	const std::vector<unsigned char>& referenceLast,
	const std::vector<unsigned char>& decodedFirst,
	const std::vector<unsigned char>& decodedLast,
	int width,
	int height) const
{
	SmokeFrameErrors result;
	result.direct = (
		CalculateFrameError(referenceFirst, decodedFirst, width, height, false, false) +
		CalculateFrameError(referenceLast, decodedLast, width, height, false, false)) * 0.5;
	result.flipped = (
		CalculateFrameError(referenceFirst, decodedFirst, width, height, true, false) +
		CalculateFrameError(referenceLast, decodedLast, width, height, true, false)) * 0.5;
	result.swapped = (
		CalculateFrameError(referenceFirst, decodedFirst, width, height, false, true) +
		CalculateFrameError(referenceLast, decodedLast, width, height, false, true)) * 0.5;
	return result;
}

// MP4動画の先頭・末尾を復号し、画素と寸法の一致を判定します。
CaptureManager::SmokeFrameErrors CaptureManager::VerifySmokeVideo(bool videoExists)
{
	SmokeFrameErrors result;
	if (!videoExists || smokeMainVideoPath_.extension() != ".mp4") {
		return result;
	}

	std::vector<unsigned char> decodedFirst;
	std::vector<unsigned char> decodedLast;
	int decodedWidth = 0;
	int decodedHeight = 0;
	result.valid = DecodeMp4FirstAndLastFrame(
		smokeMainVideoPath_, decodedFirst, decodedLast, decodedWidth, decodedHeight);
	if (!result.valid) {
		return result;
	}

	result = CompareSmokeFrames(
		smokeFirstVideoFrame_, smokeLastVideoFrame_,
		decodedFirst, decodedLast, decodedWidth, decodedHeight);
	result.valid =
		decodedWidth == smokeVideoWidth_ &&
		decodedHeight == smokeVideoHeight_ &&
		result.direct < 55.0 &&
		result.direct < result.flipped * 0.85 &&
		result.direct < result.swapped * 0.95;

	const std::filesystem::path decodedFirstPath =
		GetCaptureDirectory("Screenshots") / "ribonin_challenge_mp4_decoded_first.bmp";
	const std::filesystem::path decodedLastPath =
		GetCaptureDirectory("Screenshots") / "ribonin_challenge_mp4_decoded_last.bmp";
	result.valid = result.valid &&
		SavePixelsAsBmp(decodedFirstPath, decodedFirst, decodedWidth, decodedHeight) &&
		SavePixelsAsBmp(decodedLastPath, decodedLast, decodedWidth, decodedHeight);
	return result;
}

// リプレイ動画の先頭・末尾を復号し、フレーム数と画素の一致を判定します。
CaptureManager::SmokeFrameErrors CaptureManager::VerifySmokeReplay(bool replayExists)
{
	SmokeFrameErrors result;
	if (!replayExists || smokeReplayPath_.extension() != ".mp4") {
		return result;
	}

	std::vector<unsigned char> decodedFirst;
	std::vector<unsigned char> decodedLast;
	int decodedWidth = 0;
	int decodedHeight = 0;
	result.valid = DecodeMp4FirstAndLastFrame(
		smokeReplayPath_,
		decodedFirst,
		decodedLast,
		decodedWidth,
		decodedHeight,
		&result.decodedFrameCount);
	if (!result.valid) {
		return result;
	}

	const int decodedFrameCount = result.decodedFrameCount;
	result = CompareSmokeFrames(
		smokeFirstReplayFrame_, smokeLastReplayFrame_,
		decodedFirst, decodedLast, decodedWidth, decodedHeight);
	result.decodedFrameCount = decodedFrameCount;
	result.valid =
		result.decodedFrameCount == smokeReplayFrameCount_ &&
		smokeReplayFrameCount_ >= 20 &&
		result.direct < 55.0 &&
		result.direct < result.flipped * 0.85 &&
		result.direct < result.swapped * 0.95;

	const std::filesystem::path decodedFirstPath =
		GetCaptureDirectory("Screenshots") / "ribonin_challenge_replay_decoded_first.bmp";
	const std::filesystem::path decodedLastPath =
		GetCaptureDirectory("Screenshots") / "ribonin_challenge_replay_decoded_last.bmp";
	result.valid = result.valid &&
		SavePixelsAsBmp(decodedFirstPath, decodedFirst, decodedWidth, decodedHeight) &&
		SavePixelsAsBmp(decodedLastPath, decodedLast, decodedWidth, decodedHeight);
	return result;
}

// Debugで予約した写真・動画・リプレイを検査し、従来のログ項目を同じ順に記録します。
void CaptureManager::VerifyDebugSmoke()
{
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
	const SmokeFrameErrors video = VerifySmokeVideo(videoExists);
	const SmokeFrameErrors replay = VerifySmokeReplay(replayExists);
	const float averageFps = smokePerformanceTime_ > 0.0f
		? static_cast<float>(smokePerformanceFrameCount_) / smokePerformanceTime_
		: 0.0f;
	const bool success =
		filesExist &&
		videoExists &&
		replayExists &&
		video.valid &&
		replay.valid &&
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
		<< " directError=" << video.direct
		<< " flippedError=" << video.flipped
		<< " swappedError=" << video.swapped
		<< " replayFrames=" << smokeReplayFrameCount_ << '/' << replay.decodedFrameCount
		<< " replayDropped=" << smokeReplayDroppedFrames_
		<< " replayDirectError=" << replay.direct
		<< " replayFlippedError=" << replay.flipped
		<< " replaySwappedError=" << replay.swapped
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
