#include "StagePauseOverlay.h"

#include "DirectXCommon.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <algorithm>
#include <utility>

namespace
{
	constexpr const char* kTabTexture = "resources/font/tab.png";
	constexpr const char* kPauseTexture = "resources/font/pause.png";
	constexpr const char* kBackToStageSelectTexture = "resources/font/backSpaceStageSelect.png";
	constexpr float kScreenMargin = 24.0f;
}

// 前方宣言したSpriteを、この実装ファイルで安全に生成します。
StagePauseOverlay::StagePauseOverlay() = default;

// 所有するSpriteを、完全な型を知る場所で解放します。
StagePauseOverlay::~StagePauseOverlay() = default;

// 本編開始前に三枚を一度だけ読み、欠けた案内を表示しないようにします。
bool StagePauseOverlay::Initialize(Engine::SpriteCommon* spriteCommon, DirectXCommon* directXCommon)
{
	Finalize();
	if (!spriteCommon || !directXCommon || !spriteCommon->Initialize(directXCommon)) {
		return false;
	}
	TextureManager* textureManager = TextureManager::GetInstance();
	if (!textureManager->LoadTexture(kTabTexture) ||
		!textureManager->LoadTexture(kPauseTexture) ||
		!textureManager->LoadTexture(kBackToStageSelectTexture)) {
		return false;
	}

	auto tabSprite = std::make_unique<Engine::Sprite>();
	if (!tabSprite->Initialize(spriteCommon, kTabTexture)) {
		return false;
	}
	tabSprite->SetTextureLeftTop({ 157.0f, 834.0f });
	tabSprite->SetTextureSize({ 1669.0f, 390.0f });

	auto pauseSprite = std::make_unique<Engine::Sprite>();
	if (!pauseSprite->Initialize(spriteCommon, kPauseTexture)) {
		return false;
	}
	pauseSprite->SetTextureLeftTop({ 309.0f, 825.0f });
	pauseSprite->SetTextureSize({ 1352.0f, 420.0f });

	auto backSprite = std::make_unique<Engine::Sprite>();
	if (!backSprite->Initialize(spriteCommon, kBackToStageSelectTexture)) {
		return false;
	}
	backSprite->SetTextureLeftTop({ 45.0f, 796.0f });
	backSprite->SetTextureSize({ 1895.0f, 419.0f });

	tabSprites_.push_back(std::move(tabSprite));
	pauseSprites_.push_back(std::move(pauseSprite));
	pauseSprites_.push_back(std::move(backSprite));
	Update(static_cast<float>(directXCommon->GetClientWidth()),
		static_cast<float>(directXCommon->GetClientHeight()));
	return true;
}

// Pause中でもTabを左上に残し、追加二枚だけを上中央と画面中央へ配置します。
void StagePauseOverlay::Update(float clientWidth, float clientHeight)
{
	if (tabSprites_.size() != 1 || pauseSprites_.size() != 2 ||
		clientWidth <= 0.0f || clientHeight <= 0.0f) {
		return;
	}

	Engine::Sprite* tabSprite = tabSprites_[0].get();
	const Vector2& tabTextureSize = tabSprite->GetTextureSize();
	const float tabWidth = (std::min)(
		clientWidth * 0.26f,
		clientHeight * 0.085f * tabTextureSize.x / tabTextureSize.y);
	const float tabHeight = tabWidth * tabTextureSize.y / tabTextureSize.x;
	tabSprite->SetPosition({ kScreenMargin, kScreenMargin });
	tabSprite->SetSize({ tabWidth, tabHeight });
	tabSprite->Update();

	Engine::Sprite* pauseSprite = pauseSprites_[0].get();
	const Vector2& pauseTextureSize = pauseSprite->GetTextureSize();
	const float pauseWidth = (std::min)(
		clientWidth * 0.50f,
		clientHeight * 0.19f * pauseTextureSize.x / pauseTextureSize.y);
	const float pauseHeight = pauseWidth * pauseTextureSize.y / pauseTextureSize.x;
	pauseSprite->SetPosition({ (clientWidth - pauseWidth) * 0.5f, clientHeight * 0.08f });
	pauseSprite->SetSize({ pauseWidth, pauseHeight });
	pauseSprite->Update();

	Engine::Sprite* backSprite = pauseSprites_[1].get();
	const Vector2& backTextureSize = backSprite->GetTextureSize();
	const float backWidth = (std::min)(
		clientWidth * 0.70f,
		clientHeight * 0.20f * backTextureSize.x / backTextureSize.y);
	const float backHeight = backWidth * backTextureSize.y / backTextureSize.x;
	backSprite->SetPosition({ (clientWidth - backWidth) * 0.5f, (clientHeight - backHeight) * 0.5f });
	backSprite->SetSize({ backWidth, backHeight });
	backSprite->Update();
}

// GPU完了を待つScene終了時に、常時・停止中の案内をまとめて解放します。
void StagePauseOverlay::Finalize()
{
	tabSprites_.clear();
	pauseSprites_.clear();
}

// 描画側がTabだけを本編中にも重ねられるよう、常時案内を返します。
const std::vector<std::unique_ptr<Engine::Sprite>>& StagePauseOverlay::GetTabSprites() const
{
	return tabSprites_;
}

// Sceneの停止状態だけで描画できるよう、停止中の二枚を返します。
const std::vector<std::unique_ptr<Engine::Sprite>>& StagePauseOverlay::GetPauseSprites() const
{
	return pauseSprites_;
}
