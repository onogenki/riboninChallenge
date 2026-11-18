#pragma once

#include <memory>
#include <vector>

namespace Engine { class DirectXCommon; }
namespace Engine { class Sprite; }
namespace Engine { class SpriteCommon; }

// クリア見出しとStageSelectへ戻る案内、入場時の白画面を所有します。
class StageClearOverlay
{
public:
	StageClearOverlay();
	~StageClearOverlay();

	// 三枚の素材を先に読み、欠けた案内のままSceneを表示しないようにします。
	bool Initialize(Engine::SpriteCommon* spriteCommon, Engine::DirectXCommon* directXCommon);
	// 文字を配置し、最後に重ねた白画面を時間とともに透明にします。
	void Update(float deltaTime, float clientWidth, float clientHeight);
	// Scene終了時に、この画面が所有するSpriteだけを解放します。
	void Finalize();
	// 共通Pipelineへ所有権を渡さず、描画する三枚の一覧を貸します。
	const std::vector<std::unique_ptr<Engine::Sprite>>& GetSprites() const;

private:
	std::vector<std::unique_ptr<Engine::Sprite>> sprites_;
	float whiteFadeElapsed_ = 0.0f;
};
