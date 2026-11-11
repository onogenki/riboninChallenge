#pragma once

#include <memory>
#include <vector>

namespace Engine { class DirectXCommon; }
namespace Engine { class Sprite; }
namespace Engine { class SpriteCommon; }

// クリア見出しとStageSelectへ戻る案内を所有し、画面サイズに合わせて配置します。
class StageClearOverlay
{
public:
	StageClearOverlay();
	~StageClearOverlay();

	// 二枚の素材を先に読み、欠けた案内のままSceneを表示しないようにします。
	bool Initialize(Engine::SpriteCommon* spriteCommon, Engine::DirectXCommon* directXCommon);
	// 画像の文字部分を上と下へ配置し、画面サイズが変わっても比率を保ちます。
	void Update(float clientWidth, float clientHeight);
	// Scene終了時に、この画面が所有するSpriteだけを解放します。
	void Finalize();
	// 共通Pipelineへ所有権を渡さず、描画する二枚の一覧を貸します。
	const std::vector<std::unique_ptr<Engine::Sprite>>& GetSprites() const;

private:
	std::vector<std::unique_ptr<Engine::Sprite>> sprites_;
};
