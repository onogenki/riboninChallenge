#pragma once

#include "MyMath.h"

// 反射描画とレーザーの当たり判定で使用する、平面鏡の情報です。
// このクラスは鏡のデータだけを持ちます。鏡を画面に描画する役割は
// Stage1（将来的には専用の描画クラス）が担当します。
class Mirror {
public:
	struct RayHit {
		bool isHit = false;
		// trueの時だけ、この面でRayを反射できます。falseなら裏面へ当たった停止点です。
		bool canReflect = false;
		float distance = 0.0f;
		Vector3 position{};
		Vector3 normal{};
	};

	Mirror() = default;
	Mirror(const Vector3& center, const Vector3& normal, float width, float height) {
		SetCenter(center);
		SetNormal(normal);
		SetSize(width, height);
	}

	void SetCenter(const Vector3& center) { center_ = center; }
	void SetNormal(const Vector3& normal) {
		const Vector3 normalized = MyMath::Normalize(normal);
		// 長さが 0 の法線には向きがないため、最後に設定した正常な向きを保ちます。
		if (MyMath::Length(normalized) > 0.0f) {
			normal_ = normalized;
		}
	}
	void SetSize(float width, float height) {
		width_ = (width > 0.01f) ? width : 0.01f;
		height_ = (height > 0.01f) ? height : 0.01f;
	}
	// falseにすると、法線が向いている表側から来たRayだけを反射します。
	void SetReflectBackface(bool isReflectBackface) { isReflectBackface_ = isReflectBackface; }

	const Vector3& GetCenter() const { return center_; }
	const Vector3& GetNormal() const { return normal_; }
	float GetWidth() const { return width_; }
	float GetHeight() const { return height_; }

	// この鏡の面を基準に反転した位置を返します。
	// 次の段階で、反射カメラの位置を求めるために使用します。
	Vector3 ReflectPoint(const Vector3& point) const {
		const Vector3 fromCenter{
			point.x - center_.x,
			point.y - center_.y,
			point.z - center_.z,
		};
		const float distanceToPlane = MyMath::Dot(fromCenter, normal_);
		const Vector3 correction = MyMath::Multiply(2.0f * distanceToPlane, normal_);
		return {
			point.x - correction.x,
			point.y - correction.y,
			point.z - correction.z,
		};
	}

	// この鏡の面で反射した方向を返します。
	// 後でレーザーの反射方向を求めるときにも、同じ計算を使用します。
	Vector3 ReflectDirection(const Vector3& direction) const {
		const float towardNormal = MyMath::Dot(direction, normal_);
		const Vector3 correction = MyMath::Multiply(2.0f * towardNormal, normal_);
		return {
			direction.x - correction.x,
			direction.y - correction.y,
			direction.z - correction.z,
		};
	}

	// 無限に続く平面ではなく、幅と高さを持つ鏡の板へRayが当たるか調べます。
	RayHit IntersectRay(
		const Vector3& origin,
		const Vector3& direction,
		float maxDistance) const {
		const Vector3 normalizedDirection = MyMath::Normalize(direction);
		if (MyMath::Length(normalizedDirection) <= 0.0001f || maxDistance <= 0.0f) {
			return {};
		}

		const float denominator = MyMath::Dot(normalizedDirection, normal_);
		if (std::abs(denominator) <= 0.0001f) {
			return {};
		}
		const Vector3 fromOriginToCenter{
			center_.x - origin.x,
			center_.y - origin.y,
			center_.z - origin.z,
		};
		const float hitDistance = MyMath::Dot(fromOriginToCenter, normal_) / denominator;
		if (hitDistance <= 0.001f || hitDistance > maxDistance) {
			return {};
		}

		const Vector3 hitPosition{
			origin.x + normalizedDirection.x * hitDistance,
			origin.y + normalizedDirection.y * hitDistance,
			origin.z + normalizedDirection.z * hitDistance,
		};
		const Vector3 fromCenter{
			hitPosition.x - center_.x,
			hitPosition.y - center_.y,
			hitPosition.z - center_.z,
		};

		// 鏡の法線と平行にならない補助軸から、鏡の横軸と縦軸を作ります。
		const Vector3 worldUp{ 0.0f, 1.0f, 0.0f };
		const Vector3 referenceAxis =
			std::abs(MyMath::Dot(normal_, worldUp)) > 0.99f
			? Vector3{ 1.0f, 0.0f, 0.0f }
			: worldUp;
		const Vector3 right = MyMath::Normalize(MyMath::Cross(referenceAxis, normal_));
		const Vector3 up = MyMath::Normalize(MyMath::Cross(normal_, right));
		const float localX = MyMath::Dot(fromCenter, right);
		const float localY = MyMath::Dot(fromCenter, up);
		if (std::abs(localX) > width_ * 0.5f || std::abs(localY) > height_ * 0.5f) {
			return {};
		}

		RayHit result{};
		result.isHit = true;
		// 裏面はRayを通過させず、反射しない停止面として扱います。
		result.canReflect = isReflectBackface_ || denominator < -0.0001f;
		result.distance = hitDistance;
		result.position = hitPosition;
		// Rayが裏側から来た場合も、Ray側を向く法線を返します。
		result.normal = denominator < 0.0f ? normal_ : MyMath::Multiply(-1.0f, normal_);
		return result;
	}

private:
	Vector3 center_{ 0.0f, 0.0f, 0.0f };
	Vector3 normal_{ 0.0f, 0.0f, 1.0f };
	float width_ = 4.0f;
	float height_ = 4.0f;
	bool isReflectBackface_ = true;
};
