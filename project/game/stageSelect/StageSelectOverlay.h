#pragma once

#include <memory>
#include <vector>

class DirectXCommon;
class Sprite;
class SpriteCommon;

// StageSelectの戻る・進む案内画像を所有し、Sceneには更新と描画の順番だけを残します。
class StageSelectOverlay
{
public:
	StageSelectOverlay();
	~StageSelectOverlay();

	// 二枚の画像とSpriteをそろえ、欠けた案内を描画へ渡さないようにします。
	bool Initialize(SpriteCommon* spriteCommon, DirectXCommon* directXCommon);
	// 透明余白を除いた案内を、画面左上と下中央へ配置します。
	void Update(float clientWidth, float clientHeight);
	// Scene終了時に所有するSpriteを解放します。
	void Finalize();
	// 描画処理には所有権を渡さず、案内画像の一覧だけを貸します。
	const std::vector<std::unique_ptr<Sprite>>& GetSprites() const;

private:
	std::vector<std::unique_ptr<Sprite>> sprites_;
};
