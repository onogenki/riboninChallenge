#pragma once

#include <memory>
#include <vector>
#include "Vector3.h"

class CarryableMirror;
namespace Engine { class Camera; }
namespace Engine { class DirectXCommon; }
namespace Engine { class Sprite; }
namespace Engine { class SpriteCommon; }

// Stage1のTab・Pause案内と、拾える鏡のE案内を所有します。
class StagePauseOverlay
{
public:
	StagePauseOverlay();
	~StagePauseOverlay();

	// 四枚すべて準備できた時だけ、通常画面とPause画面の案内を使えるようにします。
	bool Initialize(Engine::SpriteCommon* spriteCommon, Engine::DirectXCommon* directXCommon);
	// 画面サイズに合わせ、常時案内と停止中の案内を同じ比率で配置します。
	void Update(float clientWidth, float clientHeight);
	// 拾える鏡の上にE案内を置き、拾えない時は描画を止めます。
	void UpdatePickupPrompt(
		const Engine::Camera* camera,
		const CarryableMirror* mirror,
		const Vector3& playerPosition,
		bool enabled,
		float clientWidth,
		float clientHeight);
	// Scene終了時に、所有する四枚のSpriteを解放します。
	void Finalize();
	// Tab案内だけを常時描画するための一覧を貸します。
	const std::vector<std::unique_ptr<Engine::Sprite>>& GetTabSprites() const;
	// Pause中だけ上から重ねる二枚の案内を貸します。
	const std::vector<std::unique_ptr<Engine::Sprite>>& GetPauseSprites() const;
	// 拾える時だけ、鏡の位置に追従するE案内を描画側へ貸します。
	const std::vector<std::unique_ptr<Engine::Sprite>>& GetPickupSprites() const;
	bool IsPickupPromptVisible() const { return isPickupPromptVisible_; }

private:
	std::vector<std::unique_ptr<Engine::Sprite>> tabSprites_;
	std::vector<std::unique_ptr<Engine::Sprite>> pauseSprites_;
	std::vector<std::unique_ptr<Engine::Sprite>> pickupSprites_;
	bool isPickupPromptVisible_ = false;
};
