#include "StageLightWarning.h"

#include "Laser.h"
#include "MyMath.h"
#include "Object3d.h"
#include "Player.h"
#include "StageHazardLights.h"
#include "StageLightPuzzle.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace MyMath;

namespace
{
	// Playerの表面からこの距離までを、接触前の予兆範囲とします。
	constexpr float kWarningDistance = 3.0f;
	constexpr float kLaserPadding = 0.16f;
	constexpr float kMaximumWarningStrength = 0.72f;

	struct NearestLight
	{
		float distanceSquared = (std::numeric_limits<float>::max)();
		Vector3 closestPoint{};
		Vector3 segmentStart{};
		bool found = false;
	};

	// 二つの箱が離れている線分を、正確な距離計算より先に除外します。
	bool Overlaps(const AABB& left, const AABB& right)
	{
		return left.min.x <= right.max.x && left.max.x >= right.min.x &&
			left.min.y <= right.max.y && left.max.y >= right.min.y &&
			left.min.z <= right.max.z && left.max.z >= right.min.z;
	}

	// 反射済み線分のAABBを調べ、候補だけPlayer中心への最近点を計算します。
	void ConsiderSegments(
		const std::vector<LaserSegment>& segments,
		const Sphere& playerSphere,
		const AABB& playerWarningBox,
		NearestLight& nearest)
	{
		for (const LaserSegment& segment : segments) {
			const AABB segmentBox{
				{
					(std::min)(segment.start.x, segment.end.x) - kLaserPadding,
					(std::min)(segment.start.y, segment.end.y) - kLaserPadding,
					(std::min)(segment.start.z, segment.end.z) - kLaserPadding,
				},
				{
					(std::max)(segment.start.x, segment.end.x) + kLaserPadding,
					(std::max)(segment.start.y, segment.end.y) + kLaserPadding,
					(std::max)(segment.start.z, segment.end.z) + kLaserPadding,
				},
			};
			if (!Overlaps(playerWarningBox, segmentBox)) {
				continue;
			}
			const Vector3 axis = Subtract(segment.end, segment.start);
			const float lengthSquared = Dot(axis, axis);
			const float ratio = lengthSquared > 0.000001f
				? std::clamp(Dot(Subtract(playerSphere.center, segment.start), axis) / lengthSquared, 0.0f, 1.0f)
				: 0.0f;
			const Vector3 closestPoint = Add(segment.start, Multiply(ratio, axis));
			const Vector3 offset = Subtract(closestPoint, playerSphere.center);
			const float distanceSquared = Dot(offset, offset);
			if (distanceSquared < nearest.distanceSquared) {
				nearest.distanceSquared = distanceSquared;
				nearest.closestPoint = closestPoint;
				nearest.segmentStart = segment.start;
				nearest.found = true;
			}
		}
	}
}

// PlayerのColliderを広げた箱で候補を選び、反射後の線分を距離順に評価します。
void StageLightWarning::Update(
	Player& player,
	const StageLightPuzzle& lightPuzzle,
	const StageHazardLights& hazardLights,
	float deltaTime)
{
	const Sphere& playerSphere = player.GetSphere();
	const float extent = playerSphere.radius + kWarningDistance + kLaserPadding;
	const AABB playerWarningBox{
		{ playerSphere.center.x - extent, playerSphere.center.y - extent, playerSphere.center.z - extent },
		{ playerSphere.center.x + extent, playerSphere.center.y + extent, playerSphere.center.z + extent },
	};
	NearestLight nearest{};
	ConsiderSegments(lightPuzzle.GetChargeLaser().GetSegments(), playerSphere, playerWarningBox, nearest);
	ConsiderSegments(lightPuzzle.GetDoorLaser().GetSegments(), playerSphere, playerWarningBox, nearest);
	ConsiderSegments(hazardLights.GetCeilingSweepSegments(), playerSphere, playerWarningBox, nearest);
	ConsiderSegments(hazardLights.GetHorizontalMoveSegments(), playerSphere, playerWarningBox, nearest);
	ConsiderSegments(hazardLights.GetBottomPulseSegments(), playerSphere, playerWarningBox, nearest);
	ConsiderSegments(hazardLights.GetOrbitSegments(), playerSphere, playerWarningBox, nearest);

	float targetStrength = 0.0f;
	if (nearest.found) {
		const float surfaceGap = (std::max)(
			0.0f,
			std::sqrt(nearest.distanceSquared) - playerSphere.radius - kLaserPadding);
		targetStrength = kMaximumWarningStrength *
			(1.0f - std::clamp(surfaceGap / kWarningDistance, 0.0f, 1.0f));
		if (targetStrength > 0.0f) {
			Vector3 towardLight = Subtract(nearest.closestPoint, playerSphere.center);
			if (Length(towardLight) <= 0.0001f) {
				towardLight = Subtract(nearest.segmentStart, playerSphere.center);
			}
			if (Length(towardLight) > 0.0001f) {
				const Vector3 targetDirection = Normalize(towardLight);
				const float directionStep = strength_ <= 0.02f
					? 1.0f : std::clamp(deltaTime * 12.0f, 0.0f, 1.0f);
				const Vector3 blended = Lerp(direction_, targetDirection, directionStep);
				direction_ = Length(blended) > 0.0001f ? Normalize(blended) : targetDirection;
			}
		}
	}
	// 近づく時は素早く、離れる時は少し緩やかに戻し、警告のちらつきを防ぎます。
	const float response = targetStrength > strength_ ? 7.0f : 3.0f;
	strength_ += (targetStrength - strength_) * std::clamp(deltaTime * response, 0.0f, 1.0f);
	if (strength_ < 0.001f) {
		strength_ = 0.0f;
	}
	player.GetObject().SetLightWarning(direction_, strength_);
}

// 直前に見えていた予兆を保持し、死亡開始フレームに色や範囲を戻しません。
void StageLightWarning::BeginDeath()
{
	deathStartStrength_ = strength_;
}

// Lightが来た側を固定したまま、消失と一緒に予兆を球全体へ広げます。
void StageLightWarning::UpdateDeath(Player& player, float progress)
{
	strength_ = deathStartStrength_ +
		(1.0f - deathStartStrength_) * std::clamp(progress, 0.0f, 1.0f);
	player.GetObject().SetLightWarning(direction_, strength_);
}

// 復帰したPlayerに前回のLight方向や黄色い反射を残さないよう初期値へ戻します。
void StageLightWarning::Reset(Player& player)
{
	direction_ = { 0.0f, 0.0f, 1.0f };
	strength_ = 0.0f;
	deathStartStrength_ = 0.0f;
	player.GetObject().SetLightWarning(direction_, strength_);
}
