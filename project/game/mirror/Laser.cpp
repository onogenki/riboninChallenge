#include "Laser.h"

#include "Collision.h"
#include <algorithm>
#include "MyMath.h"
#include <limits>

using namespace MyMath;

void Laser::Update(const std::vector<const Mirror*>& mirrors)
{
	Update(mirrors, {}, 0.0f);
}

void Laser::Update(
	const std::vector<const Mirror*>& mirrors,
	const std::vector<OBB>& blockingObbs,
	float blockingPadding)
{
	segments_.clear();
	Vector3 rayOrigin = origin_;
	Vector3 rayDirection = Normalize(direction_);
	float remainingDistance = maxDistance_;
	constexpr float kSurfaceOffset = 0.01f;

	for (size_t reflectionCount = 0;
		reflectionCount <= maxReflectionCount_ && remainingDistance > 0.0f;
		++reflectionCount) {
		float nearestDistance = (std::numeric_limits<float>::max)();
		Mirror::RayHit nearestHit{};
		size_t nearestMirrorIndex = 0;

		for (size_t mirrorIndex = 0; mirrorIndex < mirrors.size(); ++mirrorIndex) {
			if (!mirrors[mirrorIndex]) {
				continue;
			}
			const Mirror::RayHit hit = mirrors[mirrorIndex]->IntersectRay(
				rayOrigin,
				rayDirection,
				remainingDistance);
			if (hit.isHit && hit.distance < nearestDistance) {
				nearestDistance = hit.distance;
				nearestHit = hit;
				nearestMirrorIndex = mirrorIndex;
			}
		}

		// Mirrorを調べるだけでは、壁の後ろにあるMirrorを先に反射してしまう。
		// 同じ線分上の床・壁・Doorも調べ、最も手前の遮蔽物を優先する。
		float nearestBlockingDistance = (std::numeric_limits<float>::max)();
		const Vector3 rayEnd{
			rayOrigin.x + rayDirection.x * remainingDistance,
			rayOrigin.y + rayDirection.y * remainingDistance,
			rayOrigin.z + rayDirection.z * remainingDistance,
		};
		for (const OBB& blockingObb : blockingObbs) {
			const Collision::SegmentHit hit = Collision::SegmentOBB(
				rayOrigin,
				rayEnd,
				blockingObb,
				blockingPadding);
			// 発射装置が床の表面に置かれた場合のt=0は、外側へ発射できるよう無視する。
			if (!hit.isHit || hit.t <= 0.0001f) {
				continue;
			}
			nearestBlockingDistance = (std::min)(
				nearestBlockingDistance,
				hit.t * remainingDistance);
		}

		const bool isBlockingHit = nearestBlockingDistance <= remainingDistance;
		const bool isBlockingBeforeMirror =
			isBlockingHit && nearestBlockingDistance <= nearestDistance + 0.0001f;
		if (isBlockingBeforeMirror) {
			segments_.push_back({
				rayOrigin,
				{
					rayOrigin.x + rayDirection.x * nearestBlockingDistance,
					rayOrigin.y + rayDirection.y * nearestBlockingDistance,
					rayOrigin.z + rayDirection.z * nearestBlockingDistance,
				},
				false,
				0,
			});
			break;
		}

		if (!nearestHit.isHit) {
			segments_.push_back({
				rayOrigin,
				{
					rayOrigin.x + rayDirection.x * remainingDistance,
					rayOrigin.y + rayDirection.y * remainingDistance,
					rayOrigin.z + rayDirection.z * remainingDistance,
				},
				false,
				0,
			});
			break;
		}

		segments_.push_back({
			rayOrigin,
			nearestHit.position,
			nearestHit.canReflect,
			nearestMirrorIndex,
		});
		// 裏面は反射しない板です。Lightをこの位置で止め、PlayerやSwitchへ抜けさせません。
		if (!nearestHit.canReflect) {
			break;
		}
		remainingDistance -= nearestDistance;
		rayDirection = Normalize(mirrors[nearestMirrorIndex]->ReflectDirection(rayDirection));
		rayOrigin = {
			nearestHit.position.x + rayDirection.x * kSurfaceOffset,
			nearestHit.position.y + rayDirection.y * kSurfaceOffset,
			nearestHit.position.z + rayDirection.z * kSurfaceOffset,
		};
		remainingDistance -= kSurfaceOffset;
	}
}

void Laser::ClipByObbs(const std::vector<OBB>& blockingObbs, float padding)
{
	for (size_t segmentIndex = 0; segmentIndex < segments_.size(); ++segmentIndex) {
		LaserSegment& segment = segments_[segmentIndex];
		float nearestT = 1.0f;
		for (const OBB& blockingObb : blockingObbs) {
			const Collision::SegmentHit hit = Collision::SegmentOBB(
				segment.start,
				segment.end,
				blockingObb,
				padding);
			if (hit.isHit && hit.t < nearestT) {
				nearestT = hit.t;
			}
		}
		if (nearestT >= 0.9999f) {
			continue;
		}

		// 鏡へ届く前に床・壁へ当たったため、反射せずその位置でLightを止めます。
		segment.end = {
			segment.start.x + (segment.end.x - segment.start.x) * nearestT,
			segment.start.y + (segment.end.y - segment.start.y) * nearestT,
			segment.start.z + (segment.end.z - segment.start.z) * nearestT,
		};
		segment.hitMirror = false;
		segment.mirrorIndex = 0;
		segments_.erase(segments_.begin() + static_cast<std::ptrdiff_t>(segmentIndex + 1), segments_.end());
		return;
	}
}

// このLaserが計算した全線分について、指定球との接触を一度だけ調べます。
bool Laser::IsHitSphere(const Sphere& sphere, float padding) const
{
	return IsAnySegmentHitSphere(segments_, sphere, padding);
}

// 任意のLaser線分一覧と球の接触を調べ、一本でも当たればtrueを返します。
bool Laser::IsAnySegmentHitSphere(
	const std::vector<LaserSegment>& segments,
	const Sphere& sphere,
	float padding)
{
	return std::any_of(
		segments.begin(),
		segments.end(),
		[&sphere, padding](const LaserSegment& segment)
		{
			return Collision::SegmentSphere(
				segment.start,
				segment.end,
				sphere,
				padding).isHit;
		});
}
