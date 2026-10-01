#include "DebugViewportPlacement.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "MyMath.h"
#include <algorithm>
#include <cmath>

// 逆ViewProjection行列で、NDC座標をワールド座標へ戻します。
bool DebugViewportPlacement::TransformCoord(
	const Vector3& point,
	const Matrix4x4& matrix,
	Vector3& outPoint)
{
	const float x = point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0];
	const float y = point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1];
	const float z = point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2];
	const float w = point.x * matrix.m[0][3] + point.y * matrix.m[1][3] + point.z * matrix.m[2][3] + matrix.m[3][3];
	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(w) ||
		std::abs(w) <= 0.0001f) {
		return false;
	}
	outPoint = { x / w, y / w, z / w };
	return std::isfinite(outPoint.x) && std::isfinite(outPoint.y) && std::isfinite(outPoint.z);
}

// Game ViewのMouse位置から、Y=0の仮想床へ置く3D座標を作ります。
bool DebugViewportPlacement::TryGetWorldPosition(
	Camera* camera,
	float screenX,
	float screenY,
	Vector3& outPosition)
{
	if (!camera) {
		return false;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight) ||
		rectWidth <= 0.0f || rectHeight <= 0.0f) {
		return false;
	}

	const float normalizedX = (screenX - rectX) / rectWidth;
	const float normalizedY = (screenY - rectY) / rectHeight;
	if (normalizedX < 0.0f || normalizedX > 1.0f || normalizedY < 0.0f || normalizedY > 1.0f) {
		return false;
	}

	const float ndcX = normalizedX * 2.0f - 1.0f;
	const float ndcY = 1.0f - normalizedY * 2.0f;
	const Matrix4x4 inverseViewProjection = MyMath::Inverse(camera->GetViewProjectionMatrix());
	Vector3 nearPoint{};
	Vector3 farPoint{};
	if (!TransformCoord({ ndcX, ndcY, 0.0f }, inverseViewProjection, nearPoint) ||
		!TransformCoord({ ndcX, ndcY, 1.0f }, inverseViewProjection, farPoint)) {
		return false;
	}

	const Vector3 rayDirection{
		farPoint.x - nearPoint.x,
		farPoint.y - nearPoint.y,
		farPoint.z - nearPoint.z,
	};
	constexpr float kPlacementHeight = 0.0f;
	if (std::abs(rayDirection.y) > 0.0001f) {
		const float t = (kPlacementHeight - nearPoint.y) / rayDirection.y;
		if (t >= 0.0f) {
			outPosition = {
				nearPoint.x + rayDirection.x * t,
				kPlacementHeight,
				nearPoint.z + rayDirection.z * t,
			};
			return true;
		}
	}

	const float length = (std::max)(MyMath::Length(rayDirection), 0.0001f);
	outPosition = {
		nearPoint.x + rayDirection.x / length * 6.0f,
		nearPoint.y + rayDirection.y / length * 6.0f,
		nearPoint.z + rayDirection.z / length * 6.0f,
	};
	return true;
}

// Game ViewのMouse位置から、Spriteが使う画面ピクセル座標を作ります。
bool DebugViewportPlacement::TryGetSpritePosition(
	DirectXCommon* directXCommon,
	float screenX,
	float screenY,
	Vector2& outPosition)
{
	if (!directXCommon) {
		return false;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight) ||
		rectWidth <= 0.0f || rectHeight <= 0.0f) {
		return false;
	}

	const float normalizedX = (screenX - rectX) / rectWidth;
	const float normalizedY = (screenY - rectY) / rectHeight;
	if (normalizedX < 0.0f || normalizedX > 1.0f || normalizedY < 0.0f || normalizedY > 1.0f) {
		return false;
	}

	outPosition = {
		normalizedX * static_cast<float>(directXCommon->GetClientWidth()),
		normalizedY * static_cast<float>(directXCommon->GetClientHeight()),
	};
	return true;
}
