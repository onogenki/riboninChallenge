#pragma once

#include <atomic>
#include <deque>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// スクリーンショット、動画録画、リプレイ保存を管理するシングルトン。
// GPUからの読み戻し後のエンコードはワーカースレッドで行い、描画を止めない。
class CaptureManager
{
public:
	static CaptureManager* GetInstance();

	CaptureManager(const CaptureManager&) = delete;
	CaptureManager& operator=(const CaptureManager&) = delete;

	// 録画・リプレイ用のワーカースレッドを開始する。
	void Initialize();
	// 保留中のエンコードを完了してからワーカースレッドを停止する。
	void Finalize();
	// 撮影・録画・リプレイ保存を操作するImGuiを描画する。
	void DrawImGui();
	// PostDraw後にGPUバックバッファを読み戻して各キャプチャ要求を処理する。
	void UpdateAfterDraw();

	bool IsRecording() const { return isRecording_; }
	const std::string& GetLastMessage() const { return lastMessage_; }

private:
	// UIで選択できる動画コンテナ形式。
	enum class VideoFormat {
		Avi,
		Mp4,
	};

	// 現在フレームを書き込んでいるエンコーダー。
	enum class ActiveVideoWriter {
		None,
		Avi,
		Mp4,
	};

	// メモリ上のリプレイバッファへ圧縮して保持する1フレーム。
	struct ReplayFrame {
		std::vector<unsigned char> encodedPixels;
		int width = 0;
		int height = 0;
	};

	// リプレイ用に縮小・圧縮する前のピクセルデータ。
	struct ReplayEncodeJob {
		std::shared_ptr<const std::vector<unsigned char>> pixels;
		int sourceWidth = 0;
		int sourceHeight = 0;
		int width = 0;
		int height = 0;
		unsigned long long generation = 0;
	};

	// GPU読み戻しが必要な要求をフレーム単位でキューイングする。
	struct PendingCaptureRequest {
		bool photo = false;
		bool startRecording = false;
		bool recordingFrame = false;
		bool replayFrame = false;
		unsigned long long recordingGeneration = 0;
	};

	// 動画エンコーダースレッドへ渡す1フレーム。
	struct VideoEncodeJob {
		std::shared_ptr<const std::vector<unsigned char>> pixels;
		int width = 0;
		int height = 0;
		int frameIndex = 0;
	};

	CaptureManager() = default;
	~CaptureManager() = default;

	std::filesystem::path GetCaptureDirectory(const char* folderName) const;
	bool CreateCaptureDirectories() const;
	std::string MakeTimestampString() const;
	// 現在の描画結果をRGBAピクセル配列としてCPUメモリへ読み戻す。
	bool CapturePixels(std::vector<unsigned char>& pixels, int& width, int& height) const;
	bool SavePixelsAsBmp(
		const std::filesystem::path& filePath,
		const std::vector<unsigned char>& pixels,
		int width,
		int height) const;
	bool SavePixelsAsPng(
		const std::filesystem::path& filePath,
		const std::vector<unsigned char>& pixels,
		int width,
		int height) const;
	bool BeginRecordingAvi(const std::filesystem::path& filePath, int width, int height, int frameRate);
	bool AppendRecordingFrame(const std::vector<unsigned char>& pixels, int width, int height, int frameIndex);
	void EndRecordingAvi();
	bool BeginRecordingMp4(const std::filesystem::path& filePath, int width, int height, int frameRate);
	bool AppendRecordingFrameMp4(const std::vector<unsigned char>& pixels, int width, int height, int frameIndex);
	void EndRecordingMp4();
	bool BeginSelectedVideo(
		const std::filesystem::path& directory,
		const std::string& filePrefix,
		int width,
		int height,
		int frameRate);
	bool AppendActiveVideoFrame(const std::vector<unsigned char>& pixels, int width, int height, int frameIndex);
	// 録画フレームを非同期でエンコードするワーカーを管理する。
	void StartVideoEncoder();
	void StopVideoEncoder();
	void WaitForVideoEncoder();
	void VideoEncoderWorker();
	bool QueueVideoFrame(
		std::shared_ptr<const std::vector<unsigned char>> pixels,
		int width,
		int height,
		int frameIndex);
	void EndActiveVideo();
	// リングバッファ内の直近フレームを動画ファイルとして保存する。
	bool SaveReplayClip();
	bool SaveReplayFrames(std::vector<ReplayFrame> frames);
	void WaitForReplaySave();
	void FinishReplaySaveIfReady();
	void StopRecording(const std::string& message);
	// 次の描画完了後に保存するスクリーンショットを一枚予約する。
	void QueuePhotoCapture();
	void ProcessCompletedCaptures(bool waitForGpu = false);
	void ProcessCapturedFrame(
		const PendingCaptureRequest& request,
		std::vector<unsigned char> pixels,
		int width,
		int height);
	void UpdateReplayBuffer(
		std::shared_ptr<const std::vector<unsigned char>> pixels,
		int width,
		int height);
	// リプレイ用フレームを圧縮する専用ワーカーを管理する。
	void StartReplayEncoder();
	void StopReplayEncoder();
	void ReplayEncoderWorker();
	bool EncodeReplayFrame(const ReplayEncodeJob& job, ReplayFrame& frame) const;
	bool DecodeReplayFrame(const ReplayFrame& frame, std::vector<unsigned char>& pixels) const;
	void ClearReplayBuffer();
	void UpdateSmokeBeforeCapture();
	void UpdateSmokeAfterCapture(bool captured, bool visiblyNonBlack);
	void FinishSmoke(bool success, const std::string& message);
	bool IsVisiblyNonBlack(const std::vector<unsigned char>& pixels, int width, int height) const;
	bool DecodeMp4FirstAndLastFrame(
		const std::filesystem::path& filePath,
		std::vector<unsigned char>& firstFrame,
		std::vector<unsigned char>& lastFrame,
		int& width,
		int& height,
		int* frameCount = nullptr) const;
	double CalculateFrameError(
		const std::vector<unsigned char>& reference,
		const std::vector<unsigned char>& decoded,
		int width,
		int height,
		bool flipVertically,
		bool swapRedBlue) const;
	std::vector<unsigned char> ResizePixels(
		const std::vector<unsigned char>& pixels,
		int sourceWidth,
		int sourceHeight,
		int targetWidth,
		int targetHeight) const;

	bool initialized_ = false;
	// GPU読み戻し前を含めた、未保存スクリーンショットの予約数。
	size_t photoRequestCount_ = 0;
	bool photoCapturePending_ = false;
	bool recordingStartRequested_ = false;
	bool recordingStartCapturePending_ = false;
	bool isRecording_ = false;
	unsigned long long recordingGeneration_ = 0;
	bool replayBufferEnabled_ = true;
	float recordingTime_ = 0.0f;
	float recordingFrameTimer_ = 0.0f;
	float replayFrameTimer_ = 0.0f;
	int recordingFrameIndex_ = 0;
	int recordingVideoWidth_ = 0;
	int recordingVideoHeight_ = 0;
	int recordingFramesPerSecond_ = 10;
	void* recordingAviFile_ = nullptr;
	void* recordingAviStream_ = nullptr;
	bool aviInitialized_ = false;
	void* recordingMp4SinkWriter_ = nullptr;
	unsigned long recordingMp4StreamIndex_ = 0;
	long long recordingMp4SampleDuration_ = 0;
	VideoFormat selectedVideoFormat_ = VideoFormat::Mp4;
	ActiveVideoWriter activeVideoWriter_ = ActiveVideoWriter::None;
	std::filesystem::path recordingVideoPath_;
	std::filesystem::path lastScreenshotPath_;
	std::filesystem::path lastVideoPath_;
	std::filesystem::path lastReplayPath_;
	// GPU読み戻し待ちの撮影要求。
	std::deque<PendingCaptureRequest> pendingCaptureRequests_;
	size_t droppedCaptureFrames_ = 0;
	// 録画用エンコーダーへ渡すフレームキュー。
	std::deque<VideoEncodeJob> videoEncodeJobs_;
	std::mutex videoEncodeMutex_;
	std::condition_variable videoEncodeWorkCondition_;
	std::condition_variable videoEncodeIdleCondition_;
	std::thread videoEncoderThread_;
	std::atomic<bool> videoEncodeFailed_{ false };
	bool videoEncoderStopping_ = false;
	size_t videoEncoderActiveJobs_ = 0;
	// 常に直近kReplaySeconds_秒だけを保持するリプレイリングバッファ。
	std::deque<ReplayFrame> replayFrames_;
	std::deque<ReplayEncodeJob> replayEncodeJobs_;
	std::mutex replayMutex_;
	std::condition_variable replayWorkCondition_;
	std::condition_variable replayIdleCondition_;
	std::thread replayEncoderThread_;
	std::thread replaySaveThread_;
	std::atomic<bool> replaySaveInProgress_{ false };
	bool replaySaveSucceeded_ = false;
	bool replayEncoderStopping_ = false;
	size_t replayActiveJobs_ = 0;
	size_t replayDroppedFrames_ = 0;
	unsigned long long replayGeneration_ = 0;
	int replayTargetWidth_ = 0;
	int replayTargetHeight_ = 0;
	std::string lastMessage_;
	// キャプチャ機能の自動検証で使用する状態。
	bool smokeEnabled_ = false;
	bool performanceBaselineEnabled_ = false;
	bool smokeFinished_ = false;
	int smokeFrame_ = 0;
	int smokeDebugFrame_ = 0;
	int smokeCaptureAttempts_ = 0;
	int smokeVisibleCaptures_ = 0;
	int smokeSavedPhotos_ = 0;
	int smokeVideoFrameCount_ = 0;
	std::vector<std::filesystem::path> smokePhotoPaths_;
	std::filesystem::path smokeLogPath_;
	std::filesystem::path smokeMainVideoPath_;
	std::filesystem::path smokeReplayPath_;
	bool smokeTrackActiveVideo_ = false;
	std::vector<unsigned char> smokeFirstVideoFrame_;
	std::vector<unsigned char> smokeLastVideoFrame_;
	std::vector<unsigned char> smokeFirstReplayFrame_;
	std::vector<unsigned char> smokeLastReplayFrame_;
	int smokeVideoWidth_ = 0;
	int smokeVideoHeight_ = 0;
	int smokeReplayFrameCount_ = 0;
	size_t smokeReplayDroppedFrames_ = 0;
	int smokePerformanceFrameCount_ = 0;
	float smokePerformanceTime_ = 0.0f;
	float smokeMaximumDeltaTime_ = 0.0f;
	double smokeReadbackMilliseconds_ = 0.0;
	double smokeVideoMilliseconds_ = 0.0;
	double smokeReplayMilliseconds_ = 0.0;
	double smokeVideoWorkerMilliseconds_ = 0.0;
	int smokeReadbackSamples_ = 0;
	int smokeVideoSamples_ = 0;
	int smokeReplaySamples_ = 0;
	int smokeVideoWorkerSamples_ = 0;

	static constexpr int kAviRecordingFramesPerSecond_ = 10;
	static constexpr int kMp4RecordingFramesPerSecond_ = 30;
	static constexpr int kReplaySeconds_ = 30;
	static constexpr int kReplayFramesPerSecond_ = 30;
	static constexpr int kReplayWidth_ = 960;
	static constexpr int kReplayHeight_ = 540;
	static constexpr size_t kReplayMaximumQueuedFrames_ = 32;
	static constexpr size_t kVideoMaximumQueuedFrames_ = 8;
};
