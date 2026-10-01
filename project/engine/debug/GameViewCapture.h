#pragma once

#include <deque>
#include <filesystem>
#include <string>
#include <vector>

// Debug画面のGame Viewを、画像・動画・リプレイとして保存する担当です。
// Sceneはこのクラスへ更新とUI表示を依頼するだけで、保存形式の詳細を持ちません。
class GameViewCapture
{
public:
	// 保存先フォルダを準備します。
	void Initialize();
	// AVIファイルを閉じ、保持しているリプレイ画像を解放します。
	void Finalize();
	// 録画中の一枚保存と、リプレイ用画像の蓄積を更新します。
	void Update();
	// Photo・Record・ReplayのDebug操作UIを表示します。
	void DrawImGui();
	// trueなら現在Game Viewを録画しています。
	bool IsRecording() const { return isRecording_; }
	// 現在の録画時間を秒で返します。
	float GetRecordingTime() const { return recordingTime_; }

	// Game ViewのRenderTextureからRGBA画素を取り出します。
	bool CapturePixels(std::vector<unsigned char>& pixels, int& width, int& height);
	// 一枚のRGBA画素をBMPファイルとして保存します。
	bool SavePixelsAsBmp(
		const std::filesystem::path& filePath,
		const std::vector<unsigned char>& pixels,
		int width,
		int height) const;
	// 一枚のRGBA画素をAVI動画として保存します。自動確認で一枚の動画が必要な時に使います。
	bool SaveSingleFrameAvi(
		const std::filesystem::path& filePath,
		const std::vector<unsigned char>& pixels,
		int width,
		int height);
	// Captures配下の種別ごとの保存先を返します。
	std::filesystem::path GetCaptureDirectory(const char* folderName) const;
	// 保存ファイル名に使う重複しにくい日時文字列を作ります。
	std::string MakeTimestampString() const;

private:
	struct ReplayFrame
	{
		// 一枚分のGame View画像です。
		std::vector<unsigned char> pixels;
		// pixelsの横幅です。
		int width = 0;
		// pixelsの高さです。
		int height = 0;
	};

	// スクリーンショット・動画・リプレイ用フォルダを作ります。
	bool CreateDirectories() const;
	// AVI動画ファイルへの記録を開始します。
	bool BeginRecordingAvi(const std::filesystem::path& filePath, int width, int height, int frameRate = 10);
	// AVI動画へ一枚分の画素を追加します。
	bool AppendRecordingFrame(const std::vector<unsigned char>& pixels, int width, int height);
	// 開いているAVI動画を閉じます。
	void EndRecordingAvi();
	// 録画中のAVIへ必要な間隔で画素を追加します。
	void UpdateRecording();
	// 直近30秒分の縮小画像を保持します。
	void UpdateReplayBuffer();
	// 保持しているリプレイ画像をAVI動画へ保存します。
	bool SaveReplayClip();

	// trueの間、Game ViewをAVI動画として記録します。
	bool isRecording_ = false;
	// 現在の動画記録時間です。
	float recordingTime_ = 0.0f;
	// 次の動画フレームを保存するまでの時間です。
	float recordingFrameTimer_ = 0.0f;
	// 動画へ追加したフレーム数です。
	int recordingFrameIndex_ = 0;
	// 今記録している動画の保存先です。
	std::filesystem::path recordingVideoPath_;
	// 最後に保存したスクリーンショットのパスです。
	std::filesystem::path lastScreenshotPath_;
	// 最後に保存した動画のパスです。
	std::filesystem::path lastVideoPath_;
	// 最後に保存したリプレイ動画のパスです。
	std::filesystem::path lastReplayPath_;
	// 直近数秒分のGame View画像をためるキューです。
	std::deque<ReplayFrame> replayFrames_;
	// falseならリプレイ画像をためません。
	bool replayBufferEnabled_ = true;
	// 次のリプレイ画像をためるまでの時間です。
	float replayFrameTimer_ = 0.0f;
	// WindowsのAVIファイルを開いている間だけ使う内部ハンドルです。
	void* recordingAviFile_ = nullptr;
	// WindowsのAVI映像ストリームを表す内部ハンドルです。
	void* recordingAviStream_ = nullptr;
	// 記録開始時の動画横幅です。
	int recordingVideoWidth_ = 0;
	// 記録開始時の動画高さです。
	int recordingVideoHeight_ = 0;
	// Capture UIへ表示する直近の結果メッセージです。
	std::string lastMessage_;

	// リプレイとして保持する秒数です。
	static constexpr int kReplaySeconds_ = 30;
	// リプレイへ保存する一秒あたりの画像数です。
	static constexpr int kReplayFramesPerSecond_ = 5;
	// リプレイ画像の横幅です。
	static constexpr int kReplayWidth_ = 480;
	// リプレイ画像の高さです。
	static constexpr int kReplayHeight_ = 270;
};
