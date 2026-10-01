#pragma once

#include "MyMath.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Object3d;

// Debug画面でTransform・Animation・Particleの巻き戻しを自動確認するテスト道具です。
// DebugSceneは対象Objectとモデル操作関数を渡すだけで、テストの段階・ログ・終了処理はこのクラスが管理します。
class DebugTimePlaybackSmoke
{
public:
	struct State
	{
		// trueなら時間・Animation・Particleの自動確認を実行します。
		bool timePlaybackSmokeEnabled_ = false;
		// trueなら自動確認は終了済みです。
		bool timePlaybackSmokeFinished_ = false;
		// 自動確認の現在段階です。
		int timePlaybackSmokeStage_ = 0;
		// 同じ状態が安定して続いたフレーム数です。
		int timePlaybackSmokeStableFrames_ = 0;
		// 削除操作を繰り返した回数です。
		int timePlaybackSmokeDeleteIterations_ = 0;
		// 現在段階を開始してからの時間です。
		float timePlaybackSmokeStageTime_ = 0.0f;
		// 前フレームのAnimation再生時間です。
		float timePlaybackSmokePreviousAnimationTime_ = 0.0f;
		// 自動確認中のParticle回転角です。
		float timePlaybackSmokeParticleRotation_ = 0.0f;
		// 自動確認で使うTransformの基準値です。
		Transform timePlaybackSmokeOrigin_{};
		Transform timePlaybackSmokeTarget_{};
		Transform timePlaybackSmokePaused_{};
		// 削除確認で追加するモデル名です。
		std::string timePlaybackSmokeModelFile_;
		// 自動確認結果を書き出すログのパスです。
		std::filesystem::path timePlaybackSmokeLogPath_;
	};

	struct Context
	{
		// Transform・Animationを確認するDebug用モデルです。所有しません。
		Object3d* objectPlane = nullptr;
		Object3d* objectAxis = nullptr;
		// Sceneが所有するモデル一覧です。削除確認で件数を確認します。
		std::vector<std::unique_ptr<Object3d>>* normalObjects = nullptr;
		std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		// Scene初期配置分のモデル数です。
		size_t baseNormalObjectCount = 0;
		size_t baseAnimationObjectCount = 0;
		// Shelfと同じ経路でモデルを追加・削除する関数です。
		std::function<bool(const std::string&)> addModel;
		std::function<void()> clearAddedSceneModels;
		// Particle設定や選択中Emitterを、自動確認前の初期状態へ戻す関数です。
		std::function<void()> resetDebugEffects;
	};

	// 必要なモデルと初期状態を確認し、時間・Animation・Particleの自動確認を開始します。
	static void Start(
		State& state,
		bool isUiSmokeEnabled,
		const std::string& modelFile,
		const std::string& timestamp,
		const Context& context);
	// 現在の段階を一つ進め、成功または失敗ならログと終了コードを出力します。
	static void Update(State& state, const Context& context, float deltaTime);
	// 初期化中の失敗も、更新中と同じ形式で終了します。
	static void Finish(State& state, bool success, const std::string& message);
};
