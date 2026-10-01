#include "StageHazardLights.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "LaserRenderer.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace MyMath;

StageHazardLights::StageHazardLights() = default;

StageHazardLights::~StageHazardLights() = default;

// 各危険Lightが使うLaserRendererを作り、種類ごとの色と見やすい太さを設定します。
void StageHazardLights::Initialize(DirectXCommon* directXCommon)
{
	const auto createRenderer = [directXCommon](
		std::unique_ptr<LaserRenderer>& renderer,
		const Vector4& color,
		float beamWidth,
		size_t maximumSegmentCount)
	{
		renderer = std::make_unique<LaserRenderer>();
		if (!renderer->Initialize(directXCommon, maximumSegmentCount)) {
			renderer.reset();
			return;
		}
		renderer->SetColor(color);
		renderer->SetBeamWidth(beamWidth);
	};
	createRenderer(ceilingSweepRenderer_, { 0.95f, 0.20f, 1.00f, 1.0f }, 0.70f, 4);
	createRenderer(horizontalMoveRenderer_, { 1.00f, 0.82f, 0.10f, 1.0f }, 0.68f, 4);
	createRenderer(bottomPulseRenderer_, { 0.15f, 0.55f, 1.00f, 1.0f }, 0.78f, 4);
	createRenderer(bottomPulseWarningRenderer_, { 1.00f, 0.05f, 0.05f, 0.80f }, 2.40f, 1);
	createRenderer(orbitRenderer_, { 0.20f, 1.00f, 0.35f, 1.0f }, 0.74f, 12);
}

// 時刻から4種類の危険Lightを作り、反射・Player接触・軌道Lightの太さまで更新します。
void StageHazardLights::Update(
	float deltaTime,
	const Sphere* playerSphere,
	const ReflectSegments& reflectSegments)
{
	const float safeDeltaTime = (std::max)(deltaTime, 0.0f);
	time_ += safeDeltaTime;
	const auto easeInOutSine = [](float progress)
	{
		const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
		return -(std::cos(clampedProgress * std::numbers::pi_v<float>) - 1.0f) * 0.5f;
	};
	const auto easeOutSine = [](float progress)
	{
		const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
		return std::sin(clampedProgress * std::numbers::pi_v<float> * 0.5f);
	};
	const auto easeInSine = [](float progress)
	{
		const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
		return 1.0f - std::cos(clampedProgress * std::numbers::pi_v<float> * 0.5f);
	};

	// 1. 上の始点を固定し、床へ当たる先端だけを左右へ振るLightです。
	const float ceilingEndOffset = std::sin(time_ * 1.30f) * ceilingSweepDistance_;
	ceilingSweepSegments_ = {{
		ceilingSweepStart_,
		{ ceilingSweepStart_.x + ceilingEndOffset, -4.00f, ceilingSweepStart_.z },
		false,
		0,
	}};

	// 2. 三秒停止してから奥・手前へ往復する横一直線のLightです。
	const float horizontalCycleTime = std::fmod(time_, 10.0f);
	float horizontalMoveProgress = 0.0f;
	if (horizontalCycleTime >= 3.0f && horizontalCycleTime < 5.0f) {
		horizontalMoveProgress = easeInOutSine((horizontalCycleTime - 3.0f) / 2.0f);
	} else if (horizontalCycleTime >= 5.0f && horizontalCycleTime < 8.0f) {
		horizontalMoveProgress = 1.0f;
	} else if (horizontalCycleTime >= 8.0f) {
		horizontalMoveProgress = 1.0f - easeInOutSine((horizontalCycleTime - 8.0f) / 2.0f);
	}
	const Vector3 horizontalStart = Lerp(horizontalMoveNearStart_, horizontalMoveFarStart_, horizontalMoveProgress);
	horizontalMoveSegments_ = {{
		horizontalStart,
		{ horizontalStart.x - 14.0f, horizontalStart.y, horizontalStart.z },
		false,
		0,
	}};

	// 3. 下から出るLightは五秒表示・十秒停止、出現三秒前だけ床を赤く予告します。
	const float bottomPulseCycleTime = std::fmod(time_, 15.0f);
	const bool isBottomPulseActive = bottomPulseCycleTime < 5.0f;
	const bool isBottomPulseWarning = bottomPulseCycleTime >= 12.0f;
	bottomPulseSegments_.clear();
	bottomPulseWarningSegments_.clear();
	if (isBottomPulseActive) {
		bottomPulseSegments_.push_back({ bottomPulsePosition_, { bottomPulsePosition_.x, 4.50f, bottomPulsePosition_.z }, false, 0 });
	}
	if (isBottomPulseWarning) {
		bottomPulseWarningSegments_.push_back({
			{ bottomPulsePosition_.x - 1.40f, -1.96f, bottomPulsePosition_.z },
			{ bottomPulsePosition_.x + 1.40f, -1.96f, bottomPulsePosition_.z },
			false,
			0,
		});
	}

	// 4. 三本の上向きLightを回転しながら広げたり閉じたりさせます。
	const float orbitCycleProgress = std::fmod(time_, 4.0f) / 4.0f;
	const bool isOrbitClosing = orbitCycleProgress >= 0.5f;
	const float orbitRadiusProgress = isOrbitClosing
		? 1.0f - easeInSine((orbitCycleProgress - 0.5f) * 2.0f)
		: easeOutSine(orbitCycleProgress * 2.0f);
	const float orbitRadius = 1.20f + (5.00f - 1.20f) * orbitRadiusProgress;
	if (orbitRenderer_) {
		orbitRenderer_->SetBeamWidth(
			isOrbitClosing ? 0.38f + (0.74f - 0.38f) * orbitRadiusProgress : 0.74f);
	}
	orbitSegments_.clear();
	const float orbitBaseAngle = time_ * 1.80f;
	for (int lightIndex = 0; lightIndex < 3; ++lightIndex) {
		const float angle = orbitBaseAngle + std::numbers::pi_v<float> * 2.0f * static_cast<float>(lightIndex) / 3.0f;
		const Vector3 orbitPosition{
			orbitCenter_.x + std::cos(angle) * orbitRadius,
			4.50f,
			orbitCenter_.z + std::sin(angle) * orbitRadius,
		};
		orbitSegments_.push_back({ orbitPosition, { orbitPosition.x, -4.00f, orbitPosition.z }, false, 0 });
	}

	if (reflectSegments) {
		ceilingSweepSegments_ = reflectSegments(ceilingSweepSegments_);
		horizontalMoveSegments_ = reflectSegments(horizontalMoveSegments_);
		bottomPulseSegments_ = reflectSegments(bottomPulseSegments_);
		orbitSegments_ = reflectSegments(orbitSegments_);
	}

	// 反射後を含む有効な危険Lightだけ、Playerの球Colliderと接触判定します。
	isPlayerHit_ = false;
	if (!playerSphere) {
		return;
	}
	const auto isHitBySegments = [playerSphere](const std::vector<LaserSegment>& segments)
	{
		return Laser::IsAnySegmentHitSphere(segments, *playerSphere, 0.16f);
	};
	isPlayerHit_ =
		isHitBySegments(ceilingSweepSegments_) ||
		isHitBySegments(horizontalMoveSegments_) ||
		isHitBySegments(bottomPulseSegments_) ||
		isHitBySegments(orbitSegments_);
}

// 全危険Lightを同じCameraへ描画し、通常画面とMirror反射画面で同じ見た目にします。
void StageHazardLights::Draw(const Camera& camera) const
{
	if (ceilingSweepRenderer_) {
		ceilingSweepRenderer_->Draw(ceilingSweepSegments_, camera);
	}
	if (horizontalMoveRenderer_) {
		horizontalMoveRenderer_->Draw(horizontalMoveSegments_, camera);
	}
	if (bottomPulseRenderer_) {
		bottomPulseRenderer_->Draw(bottomPulseSegments_, camera);
	}
	if (bottomPulseWarningRenderer_) {
		bottomPulseWarningRenderer_->Draw(bottomPulseWarningSegments_, camera);
	}
	if (orbitRenderer_) {
		orbitRenderer_->Draw(orbitSegments_, camera);
	}
}
