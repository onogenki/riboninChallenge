#pragma once

#include "Vector2.h"
#include "Vector3.h"
#include "Matrix4x4.h"

class Camera;
class DirectXCommon;

// DebugのGame View上のMouse座標を、3Dモデル用またはSprite用の配置座標へ変換する部品です。
class DebugViewportPlacement
{
public:
	// Game ViewのMouse位置から、Y=0の仮想床へ置く3D座標を作ります。
	static bool TryGetWorldPosition(
		Camera* camera,
		float screenX,
		float screenY,
		Vector3& outPosition);
	// Game ViewのMouse位置から、Spriteが使う画面ピクセル座標を作ります。
	static bool TryGetSpritePosition(
		DirectXCommon* directXCommon,
		float screenX,
		float screenY,
		Vector2& outPosition);

private:
	// 逆ViewProjection行列で、NDC座標をワールド座標へ戻します。
	static bool TransformCoord(const Vector3& point, const Matrix4x4& matrix, Vector3& outPoint);
};
