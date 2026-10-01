#pragma once
#include"Vector3.h"
#include"Quaternion.h"

struct Transform
{
	Vector3 scale{ 1.0f, 1.0f, 1.0f };
	Vector3 rotate;//Eulerでの回転
	Vector3 translate{};
};

struct QuaternionTransform
{
	Vector3 scale{ 1.0f, 1.0f, 1.0f };
	Quaternion rotate{};
	Vector3 translate{};
};
