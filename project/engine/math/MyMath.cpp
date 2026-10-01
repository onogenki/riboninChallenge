#include "MyMath.h"
#include <cmath>
#include <cassert>

namespace MyMath {

	Vector2 AddVector2(const Vector2& left, const Vector2& right) {
		return { left.x + right.x, left.y + right.y };
	}

	Vector2 SubtractVector2(const Vector2& left, const Vector2& right) {
		return { left.x - right.x, left.y - right.y };
	}

	Vector3 Add(const Vector3& left, const Vector3& right) {
		return { left.x + right.x, left.y + right.y, left.z + right.z };
	}

	Vector3 Subtract(const Vector3& left, const Vector3& right) {
		return { left.x - right.x, left.y - right.y, left.z - right.z };
	}

	Vector4 AddVector4(const Vector4& left, const Vector4& right) {
		return { left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w };
	}

	Vector4 SubtractVector4(const Vector4& left, const Vector4& right) {
		return { left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w };
	}

	Vector2 MultiplyVector2(float scalar, const Vector2& v) {
		return { scalar * v.x, scalar * v.y };
	}

	Vector3 Multiply(float scalar, const Vector3& v) {
		return { scalar * v.x, scalar * v.y, scalar * v.z };
	}

	Vector4 MultiplyVector4(float scalar, const Vector4& v) {
		return { scalar * v.x, scalar * v.y, scalar * v.z, scalar * v.w };
	}

	float DotVector2(const Vector2& v1, const Vector2& v2) {
		return v1.x * v2.x + v1.y * v2.y;
	}

	float Dot(const Vector3& v1, const Vector3& v2) {
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	float DotVector4(const Vector4& v1, const Vector4& v2) {
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
	}

	Vector3 Cross(const Vector3& v1, const Vector3& v2) {
		return {
			v1.y * v2.z - v1.z * v2.y,
			v1.z * v2.x - v1.x * v2.z,
			v1.x * v2.y - v1.y * v2.x,
		};
	}

	float LengthVector2(const Vector2& v) {
		return std::sqrt(DotVector2(v, v));
	}

	float Length(const Vector3& v) {
		return std::sqrt(Dot(v, v));
	}

	float LengthVector4(const Vector4& v) {
		return std::sqrt(DotVector4(v, v));
	}

	Vector2 NormalizeVector2(const Vector2& v) {
		const float length = LengthVector2(v);
		return std::isfinite(length) && length > 0.000001f
			? MultiplyVector2(1.0f / length, v)
			: Vector2{};
	}

	Vector3 Normalize(const Vector3& v) {
		const float length = Length(v);
		return std::isfinite(length) && length > 0.000001f
			? Multiply(1.0f / length, v)
			: Vector3{};
	}

	Vector4 NormalizeVector4(const Vector4& v) {
		const float length = LengthVector4(v);
		return std::isfinite(length) && length > 0.000001f
			? MultiplyVector4(1.0f / length, v)
			: Vector4{};
	}

	Vector2 LerpVector2(const Vector2& v1, const Vector2& v2, float t) {
		return AddVector2(v1, MultiplyVector2(t, SubtractVector2(v2, v1)));
	}

	Vector3 Lerp(const Vector3& v1, const Vector3& v2, float t) {
		return Add(v1, Multiply(t, Subtract(v2, v1)));
	}

	Vector4 LerpVector4(const Vector4& v1, const Vector4& v2, float t) {
		return AddVector4(v1, MultiplyVector4(t, SubtractVector4(v2, v1)));
	}

	// 浮動小数点の丸め誤差を考慮して、二つの値が十分近いかを調べます。
	bool IsNearlyEqual(float lhs, float rhs, float tolerance) {
		return std::abs(lhs - rhs) <= tolerance;
	}

	bool IsNearlyEqualVector2(const Vector2& lhs, const Vector2& rhs, float tolerance) {
		return IsNearlyEqual(lhs.x, rhs.x, tolerance) &&
			IsNearlyEqual(lhs.y, rhs.y, tolerance);
	}

	// X・Y・Zの全成分が十分近い時だけ、二つのVector3を同じと扱います。
	bool IsNearlyEqual(const Vector3& lhs, const Vector3& rhs, float tolerance) {
		return IsNearlyEqual(lhs.x, rhs.x, tolerance) &&
			IsNearlyEqual(lhs.y, rhs.y, tolerance) &&
			IsNearlyEqual(lhs.z, rhs.z, tolerance);
	}

	bool IsNearlyEqualVector4(const Vector4& lhs, const Vector4& rhs, float tolerance) {
		return IsNearlyEqual(lhs.x, rhs.x, tolerance) &&
			IsNearlyEqual(lhs.y, rhs.y, tolerance) &&
			IsNearlyEqual(lhs.z, rhs.z, tolerance) &&
			IsNearlyEqual(lhs.w, rhs.w, tolerance);
	}

	// 拡大率・回転・位置の全成分が十分近い時だけ、二つのTransformを同じと扱います。
	bool IsNearlyEqual(const Transform& lhs, const Transform& rhs, float tolerance) {
		return IsNearlyEqual(lhs.scale, rhs.scale, tolerance) &&
			IsNearlyEqual(lhs.rotate, rhs.rotate, tolerance) &&
			IsNearlyEqual(lhs.translate, rhs.translate, tolerance);
	}

	Matrix4x4 MakeIdentity4x4() {
		Matrix4x4 result = {};
		result.m[0][0] = 1; result.m[1][1] = 1;
		result.m[2][2] = 1; result.m[3][3] = 1;
		return result;
	}

	Matrix4x4 Multiply(const Matrix4x4& matrix1, const Matrix4x4& matrix2) {
		Matrix4x4 result = {};
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				for (int k = 0; k < 4; k++) {
					result.m[i][j] += matrix1.m[i][k] * matrix2.m[k][j];
				}
			}
		}
		return result;
	}

	Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
		Matrix4x4 result = {};
		result.m[0][0] = scale.x;
		result.m[1][1] = scale.y;
		result.m[2][2] = scale.z;
		result.m[3][3] = 1.0f;
		return result;
	}

	Matrix4x4 MakeRotateXMatrix(float theta) {
		float s = std::sin(theta);
		float c = std::cos(theta);
		Matrix4x4 result = MakeIdentity4x4();
		result.m[1][1] = c;  result.m[1][2] = s;
		result.m[2][1] = -s; result.m[2][2] = c;
		return result;
	}

	Matrix4x4 MakeRotateYMatrix(float theta) {
		float s = std::sin(theta);
		float c = std::cos(theta);
		Matrix4x4 result = MakeIdentity4x4();
		result.m[0][0] = c;  result.m[0][2] = -s;
		result.m[2][0] = s;  result.m[2][2] = c;
		return result;
	}

	Matrix4x4 MakeRotateZMatrix(float theta) {
		float s = std::sin(theta);
		float c = std::cos(theta);
		Matrix4x4 result = MakeIdentity4x4();
		result.m[0][0] = c;  result.m[0][1] = s;
		result.m[1][0] = -s; result.m[1][1] = c;
		return result;
	}

	Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
		Matrix4x4 result = MakeIdentity4x4();
		result.m[3][0] = translate.x;
		result.m[3][1] = translate.y;
		result.m[3][2] = translate.z;
		return result;
	}

	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
		Matrix4x4 matScale = MakeScaleMatrix(scale);
		Matrix4x4 matRotX = MakeRotateXMatrix(rotate.x);
		Matrix4x4 matRotY = MakeRotateYMatrix(rotate.y);
		Matrix4x4 matRotZ = MakeRotateZMatrix(rotate.z);
		Matrix4x4 matRot = Multiply(Multiply(matRotX, matRotY), matRotZ);
		Matrix4x4 matTrans = MakeTranslateMatrix(translate);

		return Multiply(Multiply(matScale, matRot), matTrans);
	}

	Matrix4x4 MakeRotateMatrix(const Quaternion& q)
	{
		Matrix4x4 result = MakeIdentity4x4();
		float xx = q.x * q.x;
		float yy = q.y * q.y;
		float zz = q.z * q.z;
		float ww = q.w * q.w;
		float xy = q.x * q.y;
		float xz = q.x * q.z;
		float xw = q.x * q.w;
		float yz = q.y * q.z;
		float yw = q.y * q.w;
		float zw = q.z * q.w;

		result.m[0][0] = 1.0f - 2.0f * (yy + zz);
		result.m[0][1] = 2.0f * (xy + zw);
		result.m[0][2] = 2.0f * (xz - yw);

		result.m[1][0] = 2.0f * (xy - zw);
		result.m[1][1] = 1.0f - 2.0f * (xx + zz);
		result.m[1][2] = 2.0f * (yz + xw);

		result.m[2][0] = 2.0f * (xz + yw);
		result.m[2][1] = 2.0f * (yz - xw);
		result.m[2][2] = 1.0f - 2.0f * (xx + yy);

		return result;
	}

	Matrix4x4 MakeAffineMatrixQuaternion(const Vector3& scale, const Quaternion& rotate, const Vector3& translate) {
		Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
		Matrix4x4 rotateMatrix = MakeRotateMatrix(rotate);
		Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);
		return Multiply(scaleMatrix, Multiply(rotateMatrix, translateMatrix));
	}

	Quaternion MultiplyQuaternion(const Quaternion& left, const Quaternion& right) {
		return {
			left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
			left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
			left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
			left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
		};
	}

	float DotQuaternion(const Quaternion& left, const Quaternion& right) {
		return left.x * right.x + left.y * right.y + left.z * right.z + left.w * right.w;
	}

	Quaternion NormalizeQuaternion(const Quaternion& quaternion) {
		const float length = std::sqrt(DotQuaternion(quaternion, quaternion));
		if (!std::isfinite(length) || length <= 0.000001f) {
			return {};
		}
		return {
			quaternion.x / length,
			quaternion.y / length,
			quaternion.z / length,
			quaternion.w / length,
		};
	}

	Quaternion ConjugateQuaternion(const Quaternion& quaternion) {
		return { -quaternion.x, -quaternion.y, -quaternion.z, quaternion.w };
	}

	Quaternion LerpQuaternion(const Quaternion& q0, const Quaternion& q1, float t) {
		return NormalizeQuaternion({
			q0.x + (q1.x - q0.x) * t,
			q0.y + (q1.y - q0.y) * t,
			q0.z + (q1.z - q0.z) * t,
			q0.w + (q1.w - q0.w) * t,
		});
	}

	Transform LerpTransform(const Transform& from, const Transform& to, float t) {
		return {
			Lerp(from.scale, to.scale, t),
			Lerp(from.rotate, to.rotate, t),
			Lerp(from.translate, to.translate, t),
		};
	}

	Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t) {
		float dot = DotQuaternion(q0, q1);
		Quaternion q1_ = q1;

		// 内積が負の場合は、逆回転を防ぐために反転
		if (dot < 0.0f) {
			q1_ = { -q1.x, -q1.y, -q1.z, -q1.w };
			dot = -dot;
		}

		// 内積が1に近い（角度がほぼ0）場合はゼロ除算を防ぐため線形補間
		if (dot >= 1.0f - 0.0005f) {
			return LerpQuaternion(q0, q1_, t);
		}

		float theta_0 = std::acos(dot);
		float theta = theta_0 * t;
		float sin_theta = std::sin(theta);
		float sin_theta_0 = std::sin(theta_0);

		float s0 = std::cos(theta) - dot * sin_theta / sin_theta_0;
		float s1 = sin_theta / sin_theta_0;

		return {
			(q0.x * s0) + (q1_.x * s1),
			(q0.y * s0) + (q1_.y * s1),
			(q0.z * s0) + (q1_.z * s1),
			(q0.w * s0) + (q1_.w * s1)
		};
	}

	float Cot(float x) {
		return 1.0f / std::tan(x);
	}

	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
		Matrix4x4 result = {};
		float cotFov = Cot(fovY / 2.0f);
		result.m[0][0] = cotFov / aspectRatio;
		result.m[1][1] = cotFov;
		result.m[2][2] = farClip / (farClip - nearClip);
		result.m[2][3] = 1.0f;
		result.m[3][2] = -nearClip * farClip / (farClip - nearClip);
		return result;
	}

	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
		Matrix4x4 result = {};
		result.m[0][0] = 2.0f / (right - left);
		result.m[1][1] = 2.0f / (top - bottom);
		result.m[2][2] = 1.0f / (farClip - nearClip);
		result.m[3][0] = (left + right) / (left - right);
		result.m[3][1] = (top + bottom) / (bottom - top);
		result.m[3][2] = nearClip / (nearClip - farClip);
		result.m[3][3] = 1.0f;
		return result;
	}

	Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
		Matrix4x4 result = {};
		result.m[0][0] = width / 2.0f;
		result.m[1][1] = -height / 2.0f;
		result.m[2][2] = maxDepth - minDepth;
		result.m[3][0] = left + width / 2.0f;
		result.m[3][1] = top + height / 2.0f;
		result.m[3][2] = minDepth;
		result.m[3][3] = 1.0f;
		return result;
	}

	Matrix4x4 Inverse(const Matrix4x4& m) {
		Matrix4x4 result = {};
		float det =
			m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2] -
			m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1] - m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2] -
			m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2] +
			m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1] + m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2] +
			m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2] -
			m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1] - m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2] -
			m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0] - m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0] +
			m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0] + m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0];

		if (det == 0) return result;
		det = 1.0f / det;

		result.m[0][0] = det * (m.m[1][1] * m.m[2][2] * m.m[3][3] + m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[1][3] * m.m[2][1] * m.m[3][2] - m.m[1][3] * m.m[2][2] * m.m[3][1] - m.m[1][2] * m.m[2][1] * m.m[3][3] - m.m[1][1] * m.m[2][3] * m.m[3][2]);
		result.m[0][1] = det * (-m.m[0][1] * m.m[2][2] * m.m[3][3] - m.m[0][2] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[2][1] * m.m[3][2] + m.m[0][3] * m.m[2][2] * m.m[3][1] + m.m[0][2] * m.m[2][1] * m.m[3][3] + m.m[0][1] * m.m[2][3] * m.m[3][2]);
		result.m[0][2] = det * (m.m[0][1] * m.m[1][2] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[3][2] - m.m[0][3] * m.m[1][2] * m.m[3][1] - m.m[0][2] * m.m[1][1] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[3][2]);
		result.m[0][3] = det * (-m.m[0][1] * m.m[1][2] * m.m[2][3] - m.m[0][2] * m.m[1][3] * m.m[2][1] - m.m[0][3] * m.m[1][1] * m.m[2][2] + m.m[0][3] * m.m[1][2] * m.m[2][1] + m.m[0][2] * m.m[1][1] * m.m[2][3] + m.m[0][1] * m.m[1][3] * m.m[2][2]);

		result.m[1][0] = det * (-m.m[1][0] * m.m[2][2] * m.m[3][3] - m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[1][3] * m.m[2][0] * m.m[3][2] + m.m[1][3] * m.m[2][2] * m.m[3][0] + m.m[1][2] * m.m[2][0] * m.m[3][3] + m.m[1][0] * m.m[2][3] * m.m[3][2]);
		result.m[1][1] = det * (m.m[0][0] * m.m[2][2] * m.m[3][3] + m.m[0][2] * m.m[2][3] * m.m[3][0] + m.m[0][3] * m.m[2][0] * m.m[3][2] - m.m[0][3] * m.m[2][2] * m.m[3][0] - m.m[0][2] * m.m[2][0] * m.m[3][3] - m.m[0][0] * m.m[2][3] * m.m[3][2]);
		result.m[1][2] = det * (-m.m[0][0] * m.m[1][2] * m.m[3][3] - m.m[0][2] * m.m[1][3] * m.m[3][0] - m.m[0][3] * m.m[1][0] * m.m[3][2] + m.m[0][3] * m.m[1][2] * m.m[3][0] + m.m[0][2] * m.m[1][0] * m.m[3][3] + m.m[0][0] * m.m[1][3] * m.m[3][2]);
		result.m[1][3] = det * (m.m[0][0] * m.m[1][2] * m.m[2][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] + m.m[0][3] * m.m[1][0] * m.m[2][2] - m.m[0][3] * m.m[1][2] * m.m[2][0] - m.m[0][2] * m.m[1][0] * m.m[2][3] - m.m[0][0] * m.m[1][3] * m.m[2][2]);

		result.m[2][0] = det * (m.m[1][0] * m.m[2][1] * m.m[3][3] + m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[1][3] * m.m[2][0] * m.m[3][1] - m.m[1][3] * m.m[2][1] * m.m[3][0] - m.m[1][1] * m.m[2][0] * m.m[3][3] - m.m[1][0] * m.m[2][3] * m.m[3][1]);
		result.m[2][1] = det * (-m.m[0][0] * m.m[2][1] * m.m[3][3] - m.m[0][1] * m.m[2][3] * m.m[3][0] - m.m[0][3] * m.m[2][0] * m.m[3][1] + m.m[0][3] * m.m[2][1] * m.m[3][0] + m.m[0][1] * m.m[2][0] * m.m[3][3] + m.m[0][0] * m.m[2][3] * m.m[3][1]);
		result.m[2][2] = det * (m.m[0][0] * m.m[1][1] * m.m[3][3] + m.m[0][1] * m.m[1][3] * m.m[3][0] + m.m[0][3] * m.m[1][0] * m.m[3][1] - m.m[0][3] * m.m[1][1] * m.m[3][0] - m.m[0][1] * m.m[1][0] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[3][1]);
		result.m[2][3] = det * (-m.m[0][0] * m.m[1][1] * m.m[2][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] - m.m[0][3] * m.m[1][0] * m.m[2][1] + m.m[0][3] * m.m[1][1] * m.m[2][0] + m.m[0][1] * m.m[1][0] * m.m[2][3] + m.m[0][0] * m.m[1][3] * m.m[2][1]);

		result.m[3][0] = det * (-m.m[1][0] * m.m[2][1] * m.m[3][2] - m.m[1][1] * m.m[2][2] * m.m[3][0] - m.m[1][2] * m.m[2][0] * m.m[3][1] + m.m[1][2] * m.m[2][1] * m.m[3][0] + m.m[1][1] * m.m[2][0] * m.m[3][2] + m.m[1][0] * m.m[2][2] * m.m[3][1]);
		result.m[3][1] = det * (m.m[0][0] * m.m[2][1] * m.m[3][2] + m.m[0][1] * m.m[2][2] * m.m[3][0] + m.m[0][2] * m.m[2][0] * m.m[3][1] - m.m[0][2] * m.m[2][1] * m.m[3][0] - m.m[0][1] * m.m[2][0] * m.m[3][2] - m.m[0][0] * m.m[2][2] * m.m[3][1]);
		result.m[3][2] = det * (-m.m[0][0] * m.m[1][1] * m.m[3][2] - m.m[0][1] * m.m[1][2] * m.m[3][0] - m.m[0][2] * m.m[1][0] * m.m[3][1] + m.m[0][2] * m.m[1][1] * m.m[3][0] + m.m[0][1] * m.m[1][0] * m.m[3][2] + m.m[0][0] * m.m[1][2] * m.m[3][1]);
		result.m[3][3] = det * (m.m[0][0] * m.m[1][1] * m.m[2][2] + m.m[0][1] * m.m[1][2] * m.m[2][0] + m.m[0][2] * m.m[1][0] * m.m[2][1] - m.m[0][2] * m.m[1][1] * m.m[2][0] - m.m[0][1] * m.m[1][0] * m.m[2][2] - m.m[0][0] * m.m[1][2] * m.m[2][1]);

		return result;
	}

	// 4x4行列の転置（行と列を入れ替える）
	Matrix4x4 Transpose(const Matrix4x4& m) {
		Matrix4x4 result;
		for (int row = 0; row < 4; ++row) {
			for (int column = 0; column < 4; ++column) {
				result.m[row][column] = m.m[column][row];
			}
		}
		return result;
	}
}
