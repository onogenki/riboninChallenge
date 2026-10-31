#include "StageTitleOverlay.h"

#include "DirectXCommon.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>

namespace
{
	constexpr const char* kTemporaryTitleTexture = "resources/font/title.png";
	constexpr const char* kTemporaryEnterTexture = "resources/font/ENTER.png";
	constexpr float kTitleFadeSeconds = 2.0f;
	constexpr float kEnterVisibleSeconds = 5.0f;
	constexpr float kEnterHiddenSeconds = 1.0f;
}

// 前方宣言したSpriteを、この実装ファイルで安全に生成します。
StageTitleOverlay::StageTitleOverlay() = default;

// 所有するSpriteを、完全な型を知る場所で解放します。
StageTitleOverlay::~StageTitleOverlay() = default;

// 二枚とも読めた場合だけSpriteを生成し、失敗時に空の登録を残しません。
bool StageTitleOverlay::Initialize(Engine::SpriteCommon* spriteCommon, Engine::DirectXCommon* directXCommon)
{
	if (!spriteCommon || !directXCommon) {
		return false;
	}
	if (!spriteCommon->Initialize(directXCommon)) {
		return false;
	}
	Engine::TextureManager* textureManager = Engine::TextureManager::GetInstance();
	if (!textureManager->LoadTexture(kTemporaryTitleTexture) ||
		!textureManager->LoadTexture(kTemporaryEnterTexture)) {
		return false;
	}
	auto titleSprite = std::make_unique<Engine::Sprite>();
	if (!titleSprite->Initialize(spriteCommon, kTemporaryTitleTexture)) {
		return false;
	}
	// 透過余白を除いた画像部分だけを、画面上側へ配置します。
	titleSprite->SetTextureLeftTop({ 129.0f, 597.0f });
	titleSprite->SetTextureSize({ 1784.0f, 713.0f });

	auto enterSprite = std::make_unique<Engine::Sprite>();
	if (!enterSprite->Initialize(spriteCommon, kTemporaryEnterTexture)) {
		return false;
	}
	enterSprite->SetTextureLeftTop({ 379.0f, 757.0f });
	enterSprite->SetTextureSize({ 1283.0f, 419.0f });
	sprites_.push_back(std::move(titleSprite));
	sprites_.push_back(std::move(enterSprite));
	Update(0.0f,
		static_cast<float>(directXCommon->GetClientWidth()),
		static_cast<float>(directXCommon->GetClientHeight()));
	return true;
}

// Sprite自身の切り抜きサイズから配置を計算し、TitleのFadeとEnterの点滅を更新します。
void StageTitleOverlay::Update(float deltaTime, float clientWidth, float clientHeight)
{
	if (sprites_.size() != 2 || clientWidth <= 0.0f || clientHeight <= 0.0f) {
		return;
	}
	const float elapsed = (std::max)(deltaTime, 0.0f);
	fadeElapsed_ = (std::min)(fadeElapsed_ + elapsed, kTitleFadeSeconds);
	blinkElapsed_ = std::fmod(
		blinkElapsed_ + elapsed, kEnterVisibleSeconds + kEnterHiddenSeconds);

	Engine::Sprite* titleSprite = sprites_[0].get();
	// Initializeで設定した切り抜きサイズを使い、配置計算との数値の重複をなくします。
	const Vector2& titleTextureSize = titleSprite->GetTextureSize();
	const float titleWidth = (std::min)(
		clientWidth * 0.8f,
		clientHeight * 0.38f * titleTextureSize.x / titleTextureSize.y);
	const float titleHeight = titleWidth * titleTextureSize.y / titleTextureSize.x;
	titleSprite->SetPosition({ (clientWidth - titleWidth) * 0.5f, clientHeight * 0.05f });
	titleSprite->SetSize({ titleWidth, titleHeight });
	titleSprite->SetColor({ 1.0f, 1.0f, 1.0f, fadeElapsed_ / kTitleFadeSeconds });
	titleSprite->Update();

	Engine::Sprite* enterSprite = sprites_[1].get();
	const Vector2& enterTextureSize = enterSprite->GetTextureSize();
	const float enterWidth = (std::min)(
		clientWidth * 0.45f,
		clientHeight * 0.22f * enterTextureSize.x / enterTextureSize.y);
	const float enterHeight = enterWidth * enterTextureSize.y / enterTextureSize.x;
	enterSprite->SetPosition({
		(clientWidth - enterWidth) * 0.5f,
		clientHeight * 0.95f - enterHeight,
	});
	enterSprite->SetSize({ enterWidth, enterHeight });
	enterSprite->SetColor({
		1.0f, 1.0f, 1.0f,
		blinkElapsed_ < kEnterVisibleSeconds ? 1.0f : 0.0f,
	});
	enterSprite->Update();
}

// Stage1が描画を終えた後だけ、所有する二枚を解放します。
void StageTitleOverlay::Finalize()
{
	sprites_.clear();
}

// 描画側が読む一覧だけを返し、Spriteの所有権はStageTitleOverlayに残します。
const std::vector<std::unique_ptr<Engine::Sprite>>& StageTitleOverlay::GetSprites() const
{
	return sprites_;
}
