#pragma once

// @brief 行ベクトルで座標を変換する4x4行列です。移動成分はm[3][0..2]に置きます。
struct Matrix4x4
{
	float m[4][4]{};
};

