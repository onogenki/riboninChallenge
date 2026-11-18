#pragma once

#include "LaserRenderer.h"

namespace Engine { class Camera; }
namespace Engine { class DirectXCommon; }
class Player;

// Playerの現在位置だけに半透明の白い光を描き、通過した場所へは光を残しません。
class StagePlayerGlow
{
public:
	// 既存の光描画器をPlayer周囲専用に準備します。
	bool Initialize(Engine::DirectXCommon* directXCommon);
	// Playerを描く前に柔らかい白い光を置きます。
	void Draw(const Engine::Camera& camera, const Player& player);

private:
	LaserRenderer lightRenderer_{};
	bool isInitialized_ = false;
};
