#pragma once

#include <memory>
#include <vector>

class DirectXCommon;
namespace Engine { class Sprite; }
namespace Engine { class SpriteCommon; }

// 提出用Titleの二枚のSpriteを生成・所有・更新します。更新時期と描画順はStage1が決めます。
class StageTitleOverlay
{
public:
	StageTitleOverlay();
	~StageTitleOverlay();

	// 二枚とも読めた場合だけSpriteを生成し、失敗時に空の登録を残しません。
	bool Initialize(Engine::SpriteCommon* spriteCommon, DirectXCommon* directXCommon);
	// Titleは一度だけ濃くし、Enterは5秒表示と1秒非表示を繰り返します。
	void Update(float deltaTime, float clientWidth, float clientHeight);
	// Scene終了時に、二枚のSpriteを解放します。
	void Finalize();
	// Spriteの所有権は渡さず、SceneRenderPipelineの描画に必要な一覧だけを貸します。
	const std::vector<std::unique_ptr<Engine::Sprite>>& GetSprites() const;

private:
	std::vector<std::unique_ptr<Engine::Sprite>> sprites_;
	float fadeElapsed_ = 0.0f;
	float blinkElapsed_ = 0.0f;
};
