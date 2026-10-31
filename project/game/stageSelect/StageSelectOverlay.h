#pragma once

#include <memory>
#include <vector>

namespace Engine { class DirectXCommon; }
namespace Engine { class Sprite; }
namespace Engine { class SpriteCommon; }

// StageSelectの戻る・進む案内と見出し画像を所有し、Sceneには更新と描画の順番だけを残します。
class StageSelectOverlay
{
public:
	StageSelectOverlay();
	~StageSelectOverlay();

	// 三枚の画像とSpriteをそろえ、欠けた案内を描画へ渡さないようにします。
	bool Initialize(Engine::SpriteCommon* spriteCommon, Engine::DirectXCommon* directXCommon);
	// 透明余白を除いた案内を左上・下中央、見出しを上中央へ配置します。
	void Update(float clientWidth, float clientHeight);
	// Scene終了時に所有するSpriteを解放します。
	void Finalize();
	// 描画処理には所有権を渡さず、案内画像の一覧だけを貸します。
	const std::vector<std::unique_ptr<Engine::Sprite>>& GetSprites() const;

private:
	std::vector<std::unique_ptr<Engine::Sprite>> sprites_;
};
