#include "GameOverOverlay.h"

#include "DirectXCommon.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <algorithm>
#include <utility>

namespace
{
	constexpr const char* kGameOverTexture = "resources/font/GameOver.png";
	constexpr const char* kEnterRetryTexture = "resources/font/EnterRetry.png";
	constexpr const char* kBackToStageSelectTexture = "resources/font/backSpaceStageSelect.png";
}

// 前方宣言したSpriteを、この実装ファイルで安全に生成します。
GameOverOverlay::GameOverOverlay() = default;

// 所有するSpriteを、完全な型を知る場所で解放します。
GameOverOverlay::~GameOverOverlay() = default;

// 三枚の画像を読み込み、すべてのSpriteができた時だけSceneへ渡します。
bool GameOverOverlay::Initialize(Engine::SpriteCommon* spriteCommon, DirectXCommon* directXCommon)
{
	Finalize();
	if (!spriteCommon || !directXCommon || !spriteCommon->Initialize(directXCommon)) {
		return false;
	}
	TextureManager* textureManager = TextureManager::GetInstance();
	if (!textureManager->LoadTexture(kGameOverTexture) ||
		!textureManager->LoadTexture(kEnterRetryTexture) ||
		!textureManager->LoadTexture(kBackToStageSelectTexture)) {
		return false;
	}

	auto gameOverSprite = std::make_unique<Engine::Sprite>();
	if (!gameOverSprite->Initialize(spriteCommon, kGameOverTexture)) {
		return false;
	}
	gameOverSprite->SetTextureLeftTop({ 76.0f, 780.0f });
	gameOverSprite->SetTextureSize({ 1845.0f, 377.0f });

	auto retrySprite = std::make_unique<Engine::Sprite>();
	if (!retrySprite->Initialize(spriteCommon, kEnterRetryTexture)) {
		return false;
	}
	retrySprite->SetTextureLeftTop({ 244.0f, 500.0f });
	retrySprite->SetTextureSize({ 1529.0f, 941.0f });

	auto backSprite = std::make_unique<Engine::Sprite>();
	if (!backSprite->Initialize(spriteCommon, kBackToStageSelectTexture)) {
		return false;
	}
	backSprite->SetTextureLeftTop({ 48.0f, 796.0f });
	backSprite->SetTextureSize({ 1889.0f, 417.0f });

	sprites_.push_back(std::move(gameOverSprite));
	sprites_.push_back(std::move(retrySprite));
	sprites_.push_back(std::move(backSprite));
	Update(static_cast<float>(directXCommon->GetClientWidth()),
		static_cast<float>(directXCommon->GetClientHeight()));
	return true;
}

// 縦横比を保ち、読みやすい大きさに配置します。
void GameOverOverlay::Update(float clientWidth, float clientHeight)
{
	if (sprites_.size() != 3 || clientWidth <= 0.0f || clientHeight <= 0.0f) {
		return;
	}

	Engine::Sprite* gameOverSprite = sprites_[0].get();
	const Vector2& gameOverTextureSize = gameOverSprite->GetTextureSize();
	const float gameOverWidth = (std::min)(
		clientWidth * 0.70f,
		clientHeight * 0.20f * gameOverTextureSize.x / gameOverTextureSize.y);
	const float gameOverHeight = gameOverWidth * gameOverTextureSize.y / gameOverTextureSize.x;
	gameOverSprite->SetPosition({ (clientWidth - gameOverWidth) * 0.5f, clientHeight * 0.07f });
	gameOverSprite->SetSize({ gameOverWidth, gameOverHeight });
	gameOverSprite->Update();

	Engine::Sprite* retrySprite = sprites_[1].get();
	const Vector2& retryTextureSize = retrySprite->GetTextureSize();
	const float retryWidth = (std::min)(
		clientWidth * 0.42f,
		clientHeight * 0.25f * retryTextureSize.x / retryTextureSize.y);
	const float retryHeight = retryWidth * retryTextureSize.y / retryTextureSize.x;

	Engine::Sprite* backSprite = sprites_[2].get();
	const Vector2& backTextureSize = backSprite->GetTextureSize();
	const float backWidth = (std::min)(
		clientWidth * 0.72f,
		clientHeight * 0.2f * backTextureSize.x / backTextureSize.y);
	const float backHeight = backWidth * backTextureSize.y / backTextureSize.x;
	const float bottomMargin = clientHeight * 0.09f;
	const float gap = clientHeight * 0.03f;
	const float retryTop = clientHeight - bottomMargin - backHeight - gap - retryHeight;
	retrySprite->SetPosition({ (clientWidth - retryWidth) * 0.5f, retryTop });
	retrySprite->SetSize({ retryWidth, retryHeight });
	retrySprite->Update();
	backSprite->SetPosition({ (clientWidth - backWidth) * 0.5f,
		clientHeight - bottomMargin - backHeight });
	backSprite->SetSize({ backWidth, backHeight });
	backSprite->Update();
}

// GPU待機済みのScene終了時に、画面専用のSpriteをまとめて解放します。
void GameOverOverlay::Finalize()
{
	sprites_.clear();
}

// 背景の後に三枚を順に重ねられるよう、読み取り用一覧を返します。
const std::vector<std::unique_ptr<Engine::Sprite>>& GameOverOverlay::GetSprites() const
{
	return sprites_;
}
