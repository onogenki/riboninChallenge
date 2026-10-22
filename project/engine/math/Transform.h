#pragma once
#include"Vector3.h"
#include"Quaternion.h"

// @brief 拡大・Euler角回転・移動を一組にし、2D・3D・UVの行列を作る入力として渡します。
struct Transform
{
	Vector3 scale{ 1.0f, 1.0f, 1.0f };
	Vector3 rotate;//Eulerでの回転
	Vector3 translate{};
};

// @brief 骨のAnimationなど、回転をQuaternionで補間する対象の変換値を保持します。
struct QuaternionTransform
{
	Vector3 scale{ 1.0f, 1.0f, 1.0f };
	Quaternion rotate{};
	Vector3 translate{};
};
