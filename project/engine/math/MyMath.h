#pragma once
#include"Vector2.h"
#include"Vector3.h"
#include"Vector4.h"
#include"Matrix4x4.h"
#include"Transform.h"
#include <cassert>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>


// 名前空間で囲む
namespace MyMath {

	// 定義
	struct Sphere {
		Vector3 center; // 中心点
		float radius;   // 半径
	};

	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	// --- Vector2・Vector3・Vector4の計算 ---

	// 二つのVectorを加算・減算します。
	Vector2 AddVector2(const Vector2& left, const Vector2& right);
	Vector2 SubtractVector2(const Vector2& left, const Vector2& right);
	Vector3 Add(const Vector3& left, const Vector3& right);
	Vector3 Subtract(const Vector3& left, const Vector3& right);
	Vector4 AddVector4(const Vector4& left, const Vector4& right);
	Vector4 SubtractVector4(const Vector4& left, const Vector4& right);

	// スカラー倍を行います。
	Vector2 MultiplyVector2(float scalar, const Vector2& v);
	Vector3 Multiply(float scalar, const Vector3& v);
	Vector4 MultiplyVector4(float scalar, const Vector4& v);

	// 内積・長さ・正規化を計算します。
	float DotVector2(const Vector2& v1, const Vector2& v2);
	float Dot(const Vector3& v1, const Vector3& v2);
	float DotVector4(const Vector4& v1, const Vector4& v2);

	// 2本のベクトルへ垂直なベクトルを作る
	Vector3 Cross(const Vector3& v1, const Vector3& v2);

	// 長さ(ノルム)を取得します。
	float LengthVector2(const Vector2& v);
	float Length(const Vector3& v);
	float LengthVector4(const Vector4& v);

	// 正規化し、長さが0の時は零Vectorを返します。
	Vector2 NormalizeVector2(const Vector2& v);
	Vector3 Normalize(const Vector3& v);
	Vector4 NormalizeVector4(const Vector4& v);

	// 二つのVectorを指定割合で補間します。
	Vector2 LerpVector2(const Vector2& v1, const Vector2& v2, float t);
	Vector3 Lerp(const Vector3& v1, const Vector3& v2, float t);
	Vector4 LerpVector4(const Vector4& v1, const Vector4& v2, float t);

	// 誤差を含めて二つの値が等しいかを調べます。Debug確認や補間の終了判定に使います。
	bool IsNearlyEqual(float lhs, float rhs, float tolerance = 0.001f);
	bool IsNearlyEqualVector2(const Vector2& lhs, const Vector2& rhs, float tolerance = 0.001f);
	bool IsNearlyEqual(const Vector3& lhs, const Vector3& rhs, float tolerance = 0.001f);
	bool IsNearlyEqualVector4(const Vector4& lhs, const Vector4& rhs, float tolerance = 0.001f);
	bool IsNearlyEqual(const Transform& lhs, const Transform& rhs, float tolerance = 0.001f);


	// --- 行列計算 ---

	// 単位行列の作成
	Matrix4x4 MakeIdentity4x4();

	// 行列の積
	Matrix4x4 Multiply(const Matrix4x4& matrix1, const Matrix4x4& matrix2);

	// 逆行列
	Matrix4x4 Inverse(const Matrix4x4& m);

	//4x4行列の転置
	Matrix4x4 Transpose(const Matrix4x4& m);

	// --- 変換行列作成 ---

	// 拡大縮小行列の作成
	Matrix4x4 MakeScaleMatrix(const Vector3& scale);

	// X軸回転行列の作成
	Matrix4x4 MakeRotateXMatrix(float theta);

	// Y軸回転行列の作成
	Matrix4x4 MakeRotateYMatrix(float theta);

	// Z軸回転行列の作成
	Matrix4x4 MakeRotateZMatrix(float theta);

	// 平行移動行列の作成
	Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

	// アフィン変換行列の作成
	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	// --- Quaternion・Transformの計算 ---

	// Quaternionを掛け合わせ、二つの回転を合成します。
	Quaternion MultiplyQuaternion(const Quaternion& left, const Quaternion& right);
	// Quaternionの内積・正規化・共役を計算します。
	float DotQuaternion(const Quaternion& left, const Quaternion& right);
	Quaternion NormalizeQuaternion(const Quaternion& quaternion);
	Quaternion ConjugateQuaternion(const Quaternion& quaternion);
	// QuaternionとTransformを補間します。
	Quaternion LerpQuaternion(const Quaternion& q0, const Quaternion& q1, float t);
	Transform LerpTransform(const Transform& from, const Transform& to, float t);

	// Quaternionから回転行列・アフィン行列を作成します。
	Matrix4x4 MakeRotateMatrix(const Quaternion& quaternion);
	Matrix4x4 MakeAffineMatrixQuaternion(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);
	// Quaternionの球面線形補間を行います。
	Quaternion Slerp(const Quaternion& q0, const Quaternion& q1, float t);

	// --- 座標変換・投影 ---

	// 透視投影行列
	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	// 正射影行列
	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

	// ビューポート変換行列
	Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

	inline void MatrixScreenPrintf() {};

	inline constexpr int kRowHeight = 20;
	inline constexpr int kColumnWidth = 60;

	// cot 関数
	float Cot(float x);

	struct AABB
	{
		Vector3 min;
		Vector3 max;
	};

	struct OBB
	{
		Vector3 center;
		Vector3 orientations[3];
		Vector3 size;
	};

}
