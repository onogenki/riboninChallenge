#pragma once

#include <memory>
#include <vector>

class DirectXCommon;
namespace Engine { class Sprite; }
namespace Engine { class SpriteCommon; }

// GameOverの見出しと二つの操作案内を所有し、配置だけを画面サイズに合わせます。
class GameOverOverlay
{
public:
	GameOverOverlay();
	~GameOverOverlay();

	// 三枚をそろえてから描画へ渡し、案内が欠けた画面を表示しないようにします。
	bool Initialize(Engine::SpriteCommon* spriteCommon, DirectXCommon* directXCommon);
	// 画像の透明余白を除き、見出しを上、操作案内を下に並べます。
	void Update(float clientWidth, float clientHeight);
	// Scene終了時に、この画面が所有するSpriteだけを解放します。
	void Finalize();
	// 共通の描画Pipelineへ、所有権を渡さず三枚の一覧だけを貸します。
	const std::vector<std::unique_ptr<Engine::Sprite>>& GetSprites() const;

private:
	std::vector<std::unique_ptr<Engine::Sprite>> sprites_;
};
