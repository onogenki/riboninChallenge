#pragma once

#include <memory>
#include <vector>

class DirectXCommon;
class Sprite;
class SpriteCommon;

// Stage1の常時Tab案内と、停止中だけのPause・戻る案内を所有します。
class StagePauseOverlay
{
public:
	StagePauseOverlay();
	~StagePauseOverlay();

	// 三枚すべて準備できた時だけ、通常画面とPause画面の案内を使えるようにします。
	bool Initialize(SpriteCommon* spriteCommon, DirectXCommon* directXCommon);
	// 画面サイズに合わせ、常時案内と停止中の案内を同じ比率で配置します。
	void Update(float clientWidth, float clientHeight);
	// Scene終了時に、所有する三枚のSpriteを解放します。
	void Finalize();
	// Tab案内だけを常時描画するための一覧を貸します。
	const std::vector<std::unique_ptr<Sprite>>& GetTabSprites() const;
	// Pause中だけ上から重ねる二枚の案内を貸します。
	const std::vector<std::unique_ptr<Sprite>>& GetPauseSprites() const;

private:
	std::vector<std::unique_ptr<Sprite>> tabSprites_;
	std::vector<std::unique_ptr<Sprite>> pauseSprites_;
};
