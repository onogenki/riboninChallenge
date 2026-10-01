#pragma once

#include "MyMath.h"
#include <vector>

namespace Collision
{
	struct CollisionInfo
	{
		bool isCollision = false;
		Vector3 normal{};
		float penetrationDepth = 0.0f;
	};

	struct SegmentHit
	{
		bool isHit = false;
		// 線分の始点を0、終点を1とした時の衝突位置です。
		float t = 1.0f;
	};

	struct SphereObbResolution
	{
		// 一つでもOBBと重なった時にtrueになります。
		bool isCollision = false;
		// 上向きの面へ接触し、床の上に立てた時にtrueになります。
		bool isGrounded = false;
	};

	//Object3dのTransformから、回転と拡大率を反映したOBBを作成する
	MyMath::OBB MakeOBB(const Transform& transform, const Vector3& localHalfSize);
	//ローカル中心も含めて、回転と拡大率を反映したOBBを作成する
	MyMath::OBB MakeOBB(
		const Transform& transform,
		const Vector3& localCenter,
		const Vector3& localHalfSize);
	//球とOBBの衝突を判定し、球を外へ戻すための情報を返す
	CollisionInfo SphereOBB(const MyMath::Sphere& sphere, const MyMath::OBB& obb);
	// 二つの球が重なっているかを判定し、1つ目の球を外へ戻す情報を返す。
	CollisionInfo SphereSphere(const MyMath::Sphere& first, const MyMath::Sphere& second);
	// 球を複数OBBから押し戻し、位置・速度と接地状態をまとめて更新する。
	SphereObbResolution ResolveSphereObbs(
		const MyMath::Sphere& sphere,
		Vector3& position,
		Vector3& velocity,
		const std::vector<MyMath::OBB>& solidObbs,
		int solveCount = 3,
		float groundNormalThreshold = 0.5f);
	//分離軸定理を使い、1つ目のOBBを2つ目のOBBから押し出す情報を返す
	CollisionInfo ObbObb(const MyMath::OBB& first, const MyMath::OBB& second);
	// 線分がOBBへ当たる最初の位置を返す。paddingでOBBを全方向へ大きくできる。
	SegmentHit SegmentOBB(
		const Vector3& start,
		const Vector3& end,
		const MyMath::OBB& obb,
		float padding = 0.0f);
	// 3D空間の線分と球が交差する最初の位置を返す。paddingで光線の太さを加えられる。
	SegmentHit SegmentSphere(
		const Vector3& start,
		const Vector3& end,
		const MyMath::Sphere& sphere,
		float padding = 0.0f);
}
