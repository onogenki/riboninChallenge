#pragma once

#include "Vector3.h"

// Debug画面で各種Particleを確認するための設定・発生処理です。
// Sceneは発生位置と経過時間を渡すだけで、Particleの種類ごとの状態はこのクラスが所有します。
class DebugParticleEffects
{
public:
	// Debug画面で使うParticleのTexture・Group・GPU Emitter設定を準備します。
	void InitializeResources();
	// Particle設定をImGuiで編集します。embeddedがtrueなら親ウィンドウへ埋め込みます。
	void DrawImGui(const Vector3& effectPosition, bool embedded = false);
	// 有効なParticleの発生とDebug用Shortcut入力を、一フレーム分まとめて更新します。
	void UpdateFrame(const Vector3& effectPosition, float deltaTime);
	// Debug Particleを消し、各設定を無効状態へ戻します。
	void Clear();

private:
	// 有効なParticleを指定位置へ必要な間隔で発生させます。
	void Update(const Vector3& effectPosition, float deltaTime);
	// Cylinder Particleの表示・非表示を切り替えます。
	void ToggleCylinder(const Vector3& effectPosition);
	// HitとRingを一度だけ発生させます。
	void EmitHitAndRing(const Vector3& effectPosition);
	// Debug用の0・Pキー入力を読み、Particle表示・単発発生を切り替えます。
	void HandleShortcutInput(const Vector3& effectPosition);

	struct EffectControl
	{
		// trueの間だけ、この種類のParticleを発生させます。
		bool enabled = false;
		// 一度に発生させるParticle数です。
		int emitCount = 1;
		// Particleの見た目の大きさです。
		float scale = 1.0f;
		// trueならCameraを向く板として描画します。
		bool billboard = true;
	};

	// trueならCylinder Particleを表示しています。
	bool isCylinderVisible_ = false;
	EffectControl hitEffect_{ false, 8, 1.0f, true };
	EffectControl ringEffect_{ false, 3, 1.0f, true };
	EffectControl cylinderEffect_{ false, 1, 1.0f, false };
	EffectControl pillarSparkleEffect_{ false, 10, 1.0f, true };
	EffectControl lightCoreEffect_{ false, 1, 1.0f, true };
	EffectControl lightRainEffect_{ false, 8, 1.0f, true };
	EffectControl lightSpiralEffect_{ false, 24, 1.0f, true };
	// 次のParticle発生までの経過時間です。
	float emitTimer_ = 0.0f;
	// trueならCylinder設定変更後に発生し直します。
	bool refreshCylinder_ = false;
};
