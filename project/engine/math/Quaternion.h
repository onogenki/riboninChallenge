#pragma once

// @brief 回転を四つの成分で表し、Animationの回転補間にも使う四元数です。
struct Quaternion {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 1.0f;
};
