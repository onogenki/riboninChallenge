#pragma once

#include "Laser.h"
#include "MyMath.h"
#include <functional>
#include <memory>
#include <vector>

class Camera;
class DirectXCommon;
class LaserRenderer;

// Stage1内を時間で動く4種類の危険Lightを担当するゲームギミックです。
// 鏡による反射はStage1が持つルールなので、ここはコールバックで反射後の線分を受け取ります。
class StageHazardLights
{
public:
	using ReflectSegments = std::function<std::vector<LaserSegment>(const std::vector<LaserSegment>&)>;

	// forward宣言したLaserRendererを安全に生成するため、実装はcppに置きます。
	StageHazardLights();
	// forward宣言したLaserRendererを安全に破棄するため、実装はcppに置きます。
	~StageHazardLights();

	// 危険Lightごとの色・太さ・最大線分数を使い、描画用Rendererを準備します。
	void Initialize(DirectXCommon* directXCommon);

	// 時刻から危険Lightを更新し、反射・Player接触までを計算します。
	void Update(
		float deltaTime,
		const MyMath::Sphere* playerSphere,
		const ReflectSegments& reflectSegments);
	// 更新済みの全危険Lightを、通常CameraまたはMirror反射Cameraへ描画します。
	void Draw(const Camera& camera) const;
	// 各種類のLightを描画・SpotLight化する時に使う反射後の線分です。
	const std::vector<LaserSegment>& GetCeilingSweepSegments() const { return ceilingSweepSegments_; }
	const std::vector<LaserSegment>& GetHorizontalMoveSegments() const { return horizontalMoveSegments_; }
	const std::vector<LaserSegment>& GetBottomPulseSegments() const { return bottomPulseSegments_; }
	const std::vector<LaserSegment>& GetBottomPulseWarningSegments() const { return bottomPulseWarningSegments_; }
	const std::vector<LaserSegment>& GetOrbitSegments() const { return orbitSegments_; }
	// trueなら現在の危険LightがPlayerへ当たっています。
	bool IsPlayerHit() const { return isPlayerHit_; }

private:
	// 4種類の危険Lightの現在線分です。
	std::vector<LaserSegment> ceilingSweepSegments_;
	std::vector<LaserSegment> horizontalMoveSegments_;
	std::vector<LaserSegment> bottomPulseSegments_;
	std::vector<LaserSegment> bottomPulseWarningSegments_;
	std::vector<LaserSegment> orbitSegments_;
	// 同じ時刻なら同じ位置になるよう、移動経過時間だけを保存します。
	float time_ = 0.0f;
	bool isPlayerHit_ = false;

	// Stage1の最初の危険Light配置です。将来JSON化する場合も、このクラスの設定値になります。
	Vector3 ceilingSweepStart_{ 18.0f, 4.25f, 26.0f };
	float ceilingSweepDistance_ = 5.0f;
	Vector3 horizontalMoveNearStart_{ 32.0f, 1.00f, 30.0f };
	Vector3 horizontalMoveFarStart_{ 32.0f, 1.00f, 55.0f };
	Vector3 bottomPulsePosition_{ 28.0f, -1.98f, 45.0f };
	Vector3 orbitCenter_{ 24.0f, 0.0f, 62.0f };
	// 危険Lightの見た目は、このギミックが所有する専用Rendererへまとめます。
	std::unique_ptr<LaserRenderer> ceilingSweepRenderer_;
	std::unique_ptr<LaserRenderer> horizontalMoveRenderer_;
	std::unique_ptr<LaserRenderer> bottomPulseRenderer_;
	std::unique_ptr<LaserRenderer> bottomPulseWarningRenderer_;
	std::unique_ptr<LaserRenderer> orbitRenderer_;
};
