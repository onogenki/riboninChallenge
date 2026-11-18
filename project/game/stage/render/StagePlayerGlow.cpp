#include "StagePlayerGlow.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "Player.h"

// Playerの周囲にだけ使う柔らかな光を準備し、失敗時はStage開始を止められるようにします。
bool StagePlayerGlow::Initialize(Engine::DirectXCommon* directXCommon)
{
	if (!directXCommon || !lightRenderer_.Initialize(directXCommon, 1)) {
		return false;
	}
	isInitialized_ = true;
	return true;
}

// 現在のPlayer位置に白い光を一枚だけ置き、体を後から描いて周囲だけを見せます。
void StagePlayerGlow::Draw(const Engine::Camera& camera, const Player& player)
{
	if (!isInitialized_) {
		return;
	}
	const Vector3 center = player.GetSphere().center;
	const float centerY = center.y + player.GetVisualFloatOffsetY() + 0.10f;
	lightRenderer_.SetSoftGlowMode(true);
	lightRenderer_.SetBeamWidth(2.35f);
	lightRenderer_.SetColor({ 0.74f, 0.86f, 1.0f, 0.17f });
	lightRenderer_.Draw({ {
		{ center.x, centerY - 1.30f, center.z },
		{ center.x, centerY + 1.30f, center.z },
	} }, camera);
}
