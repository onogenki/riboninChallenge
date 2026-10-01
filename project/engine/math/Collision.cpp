#include "Collision.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace MyMath;

namespace
{
	float Clamp(float value, float minimum, float maximum)
	{
		if (value < minimum) {
			return minimum;
		}
		if (value > maximum) {
			return maximum;
		}
		return value;
	}

	float GetHalfSize(const OBB& obb, int axisIndex)
	{
		return axisIndex == 0 ? obb.size.x : (axisIndex == 1 ? obb.size.y : obb.size.z);
	}

	float ProjectObbRadius(const OBB& obb, const Vector3& axis)
	{
		float radius = 0.0f;
		for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
			radius += GetHalfSize(obb, axisIndex) * std::abs(Dot(axis, obb.orientations[axisIndex]));
		}
		return radius;
	}

	bool TestSeparatingAxis(const OBB& first, const OBB& second, const Vector3& candidateAxis, Collision::CollisionInfo& result)
	{
		//平行な辺同士の外積は長さ0になるため、分離軸として使わない
		const float axisLength = Length(candidateAxis);
		if (axisLength <= 0.0001f) {
			return true;
		}

		const Vector3 axis = Multiply(1.0f / axisLength, candidateAxis);
		const float firstRadius = ProjectObbRadius(first, axis);
		const float secondRadius = ProjectObbRadius(second, axis);
		const Vector3 centerDifference = Subtract(second.center, first.center);
		const float centerDistance = Dot(centerDifference, axis);
		const float overlap = firstRadius + secondRadius - std::abs(centerDistance);
		if (overlap < 0.0f) {
			return false;
		}

		//最も短く押し出せる軸を、衝突の法線と押し戻し距離に採用する
		if (overlap < result.penetrationDepth) {
			result.penetrationDepth = overlap;
			result.normal = centerDistance >= 0.0f ? Multiply(-1.0f, axis) : axis;
		}
		return true;
	}
}

MyMath::OBB Collision::MakeOBB(const Transform& transform, const Vector3& localHalfSize)
{
	return MakeOBB(transform, { 0.0f, 0.0f, 0.0f }, localHalfSize);
}

MyMath::OBB Collision::MakeOBB(
	const Transform& transform,
	const Vector3& localCenter,
	const Vector3& localHalfSize)
{
	//拡大率を除いた回転行列から、OBBの3本の軸を取り出す
	const Matrix4x4 rotationMatrix = MakeAffineMatrix(
		{ 1.0f, 1.0f, 1.0f }, transform.rotate, { 0.0f, 0.0f, 0.0f });
	const Matrix4x4 worldMatrix = MakeAffineMatrix(
		transform.scale, transform.rotate, transform.translate);

	OBB obb{};
	obb.center = {
		localCenter.x * worldMatrix.m[0][0] +
			localCenter.y * worldMatrix.m[1][0] +
			localCenter.z * worldMatrix.m[2][0] +
			worldMatrix.m[3][0],
		localCenter.x * worldMatrix.m[0][1] +
			localCenter.y * worldMatrix.m[1][1] +
			localCenter.z * worldMatrix.m[2][1] +
			worldMatrix.m[3][1],
		localCenter.x * worldMatrix.m[0][2] +
			localCenter.y * worldMatrix.m[1][2] +
			localCenter.z * worldMatrix.m[2][2] +
			worldMatrix.m[3][2],
	};
	for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
		obb.orientations[axisIndex] = Normalize({
			rotationMatrix.m[axisIndex][0],
			rotationMatrix.m[axisIndex][1],
			rotationMatrix.m[axisIndex][2],
		});
	}

	//OBBのsizeは、中心から各面までの半分の長さを表す
	obb.size = {
		localHalfSize.x * std::abs(transform.scale.x),
		localHalfSize.y * std::abs(transform.scale.y),
		localHalfSize.z * std::abs(transform.scale.z),
	};
	return obb;
}

Collision::CollisionInfo Collision::SphereOBB(const Sphere& sphere, const OBB& obb)
{
	//球の中心を、OBBの3本の軸を使ってOBBのローカル位置へ変換する
	const Vector3 fromBoxCenter = Subtract(sphere.center, obb.center);
	float localPosition[3]{};
	Vector3 closestPoint = obb.center;

	for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
		localPosition[axisIndex] = Dot(fromBoxCenter, obb.orientations[axisIndex]);
		const float halfSize = axisIndex == 0 ? obb.size.x : (axisIndex == 1 ? obb.size.y : obb.size.z);
		const float clampedPosition = Clamp(localPosition[axisIndex], -halfSize, halfSize);
		closestPoint = Add(closestPoint, Multiply(clampedPosition, obb.orientations[axisIndex]));
	}

	//OBB上の最近接点と球の中心の距離が、球の半径より短ければ衝突している
	const Vector3 fromClosestPoint = Subtract(sphere.center, closestPoint);
	const float distance = Length(fromClosestPoint);
	//接触しただけの状態も衝突として扱い、床の上で接地状態を安定させる
	if (distance > sphere.radius + 0.001f) {
		return {};
	}

	CollisionInfo result{};
	result.isCollision = true;
	if (distance > 0.0001f) {
		//球の中心がOBBの外側にある通常の場合
		result.normal = Multiply(1.0f / distance, fromClosestPoint);
		result.penetrationDepth = (std::max)(0.0f, sphere.radius - distance);
		return result;
	}

	//球の中心がOBBの内側にある場合は、最も近い面を選んで外へ押し出す
	int nearestAxisIndex = 0;
	float nearestFaceDistance = obb.size.x - std::abs(localPosition[0]);
	for (int axisIndex = 1; axisIndex < 3; ++axisIndex) {
		const float halfSize = axisIndex == 1 ? obb.size.y : obb.size.z;
		const float faceDistance = halfSize - std::abs(localPosition[axisIndex]);
		if (faceDistance < nearestFaceDistance) {
			nearestFaceDistance = faceDistance;
			nearestAxisIndex = axisIndex;
		}
	}

	const float sign = localPosition[nearestAxisIndex] >= 0.0f ? 1.0f : -1.0f;
	result.normal = Multiply(sign, obb.orientations[nearestAxisIndex]);
	result.penetrationDepth = sphere.radius + nearestFaceDistance;
	return result;
}

// 二つの球の中心距離と半径の合計を比べ、重なりと押し戻し量を返します。
Collision::CollisionInfo Collision::SphereSphere(const Sphere& first, const Sphere& second)
{
	const Vector3 centerDifference = Subtract(first.center, second.center);
	const float distance = Length(centerDifference);
	const float combinedRadius = first.radius + second.radius;
	if (distance > combinedRadius + 0.001f) {
		return {};
	}

	CollisionInfo result{};
	result.isCollision = true;
	if (distance > 0.0001f) {
		result.normal = Multiply(1.0f / distance, centerDifference);
		result.penetrationDepth = (std::max)(0.0f, combinedRadius - distance);
		return result;
	}

	// 完全に同じ位置へ出現した場合も、安定して片方を押し出せる既定方向を返します。
	result.normal = { 1.0f, 0.0f, 0.0f };
	result.penetrationDepth = (std::max)(combinedRadius, 0.0f);
	return result;
}

// 球を複数の床・壁・置物から順番に押し戻し、移動物共通の衝突解決を行います。
Collision::SphereObbResolution Collision::ResolveSphereObbs(
	const Sphere& sphere,
	Vector3& position,
	Vector3& velocity,
	const std::vector<OBB>& solidObbs,
	int solveCount,
	float groundNormalThreshold)
{
	SphereObbResolution result{};
	Sphere movingSphere = sphere;
	const int clampedSolveCount = (std::max)(solveCount, 1);

	// 斜めの箱や複数の箱に接した時も、前の押し戻し結果を次の判定へ反映します。
	for (int solveIndex = 0; solveIndex < clampedSolveCount; ++solveIndex) {
		for (const OBB& solidObb : solidObbs) {
			movingSphere.center = position;
			const CollisionInfo collision = SphereOBB(movingSphere, solidObb);
			if (!collision.isCollision) {
				continue;
			}

			result.isCollision = true;
			position.x += collision.normal.x * collision.penetrationDepth;
			position.y += collision.normal.y * collision.penetrationDepth;
			position.z += collision.normal.z * collision.penetrationDepth;
			movingSphere.center = position;

			if (collision.normal.y > groundNormalThreshold) {
				result.isGrounded = true;
			}

			// 面へ向かう速度だけを消し、着地後に落下し続けないようにします。
			const float velocityTowardSurface = Dot(velocity, collision.normal);
			if (velocityTowardSurface < 0.0f) {
				velocity.x -= collision.normal.x * velocityTowardSurface;
				velocity.y -= collision.normal.y * velocityTowardSurface;
				velocity.z -= collision.normal.z * velocityTowardSurface;
			}
		}
	}
	return result;
}

Collision::CollisionInfo Collision::ObbObb(const OBB& first, const OBB& second)
{
	//OBB同士の分離軸候補は、面法線6本と辺同士の外積9本の合計15本
	CollisionInfo result{};
	result.penetrationDepth = (std::numeric_limits<float>::max)();

	for (int firstAxisIndex = 0; firstAxisIndex < 3; ++firstAxisIndex) {
		if (!TestSeparatingAxis(first, second, first.orientations[firstAxisIndex], result)) {
			return {};
		}
	}
	for (int secondAxisIndex = 0; secondAxisIndex < 3; ++secondAxisIndex) {
		if (!TestSeparatingAxis(first, second, second.orientations[secondAxisIndex], result)) {
			return {};
		}
	}
	for (int firstAxisIndex = 0; firstAxisIndex < 3; ++firstAxisIndex) {
		for (int secondAxisIndex = 0; secondAxisIndex < 3; ++secondAxisIndex) {
			const Vector3 crossAxis = Cross(first.orientations[firstAxisIndex], second.orientations[secondAxisIndex]);
			if (!TestSeparatingAxis(first, second, crossAxis, result)) {
				return {};
			}
		}
	}

	result.isCollision = true;
	return result;
}

Collision::SegmentHit Collision::SegmentOBB(
	const Vector3& start,
	const Vector3& end,
	const OBB& obb,
	float padding)
{
	// 線分の始点と終点を、OBBの3本の軸を基準にしたローカル座標へ変換する
	const Vector3 fromCenterToStart = Subtract(start, obb.center);
	const Vector3 fromCenterToEnd = Subtract(end, obb.center);
	float startLocal[3]{};
	float endLocal[3]{};
	for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
		startLocal[axisIndex] = Dot(fromCenterToStart, obb.orientations[axisIndex]);
		endLocal[axisIndex] = Dot(fromCenterToEnd, obb.orientations[axisIndex]);
	}

	// 3軸それぞれでOBBへ入る時間と出る時間を求め、共通する時間があれば衝突している
	float entryT = 0.0f;
	float exitT = 1.0f;
	for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
		const float direction = endLocal[axisIndex] - startLocal[axisIndex];
		const float halfSize = GetHalfSize(obb, axisIndex) + padding;
		if (std::abs(direction) <= 0.0001f) {
			// この軸へ動かない場合、最初から外側なら絶対にOBBへ入れない
			if (startLocal[axisIndex] < -halfSize || startLocal[axisIndex] > halfSize) {
				return {};
			}
			continue;
		}

		float axisEntryT = (-halfSize - startLocal[axisIndex]) / direction;
		float axisExitT = (halfSize - startLocal[axisIndex]) / direction;
		if (axisEntryT > axisExitT) {
			std::swap(axisEntryT, axisExitT);
		}
		entryT = (std::max)(entryT, axisEntryT);
		exitT = (std::min)(exitT, axisExitT);
		if (entryT > exitT) {
			return {};
		}
	}

	SegmentHit result{};
	result.isHit = true;
	result.t = entryT;
	return result;
}

Collision::SegmentHit Collision::SegmentSphere(
	const Vector3& start,
	const Vector3& end,
	const Sphere& sphere,
	float padding)
{
	const Vector3 segment{
		end.x - start.x,
		end.y - start.y,
		end.z - start.z,
	};
	const Vector3 centerToStart{
		start.x - sphere.center.x,
		start.y - sphere.center.y,
		start.z - sphere.center.z,
	};
	const float radius = (std::max)(sphere.radius + padding, 0.0f);
	const float radiusSquared = radius * radius;
	const float startDistanceSquared = Dot(centerToStart, centerToStart);
	if (startDistanceSquared <= radiusSquared) {
		return { true, 0.0f };
	}

	const float segmentLengthSquared = Dot(segment, segment);
	if (segmentLengthSquared <= 0.000001f) {
		return {};
	}

	// |start + segment * t - center|^2 = radius^2 を解き、0～1の範囲を調べる。
	const float projection = Dot(centerToStart, segment);
	const float constant = startDistanceSquared - radiusSquared;
	const float discriminant =
		projection * projection - segmentLengthSquared * constant;
	if (discriminant < 0.0f) {
		return {};
	}

	const float hitT =
		(-projection - std::sqrt(discriminant)) / segmentLengthSquared;
	if (hitT < 0.0f || hitT > 1.0f) {
		return {};
	}
	return { true, hitT };
}
