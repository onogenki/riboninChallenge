#include "StageClearOverlay.h"

#include "DirectXCommon.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <algorithm>
#include <cstddef>
#include <utility>

namespace
{
	constexpr const char* kStageClearTexture = "resources/font/stageClear.png";
	constexpr const char* kBackToStageSelectTexture = "resources/font/backSpaceStageSelect.png";
	constexpr const char* kWhiteTexture = "resources/debug/white.png";
	constexpr float kWhiteFadeDuration = 0.85f;
	constexpr std::size_t kClearSpriteIndex = 0;
	constexpr std::size_t kBackSpriteIndex = 1;
	constexpr std::size_t kWhiteSpriteIndex = 2;
	constexpr std::size_t kSpriteCount = 3;
}

// 前方宣言したSpriteを、この実装ファイルで安全に生成します。
StageClearOverlay::StageClearOverlay() = default;

// 所有するSpriteを、完全な型を知る場所で解放します。
StageClearOverlay::~StageClearOverlay() = default;

// クリア見出し、戻る案内、入場時の白画面を読み、全てそろった時だけSceneへ渡します。
bool StageClearOverlay::Initialize(Engine::SpriteCommon* spriteCommon, Engine::DirectXCommon* directXCommon)
{
	Finalize();
	if (!spriteCommon || !directXCommon || !spriteCommon->Initialize(directXCommon)) {
		return false;
	}
	Engine::TextureManager* textureManager = Engine::TextureManager::GetInstance();
	if (!textureManager->LoadTexture(kStageClearTexture) ||
		!textureManager->LoadTexture(kBackToStageSelectTexture) ||
		!textureManager->LoadTexture(kWhiteTexture)) {
		return false;
	}

	auto clearSprite = std::make_unique<Engine::Sprite>();
	if (!clearSprite->Initialize(spriteCommon, kStageClearTexture)) {
		return false;
	}
	clearSprite->SetTextureLeftTop({ 20.0f, 790.0f });
	clearSprite->SetTextureSize({ 1960.0f, 430.0f });

	auto backSprite = std::make_unique<Engine::Sprite>();
	if (!backSprite->Initialize(spriteCommon, kBackToStageSelectTexture)) {
		return false;
	}
	backSprite->SetTextureLeftTop({ 48.0f, 796.0f });
	backSprite->SetTextureSize({ 1889.0f, 417.0f });
	auto whiteSprite = std::make_unique<Engine::Sprite>();
	if (!whiteSprite->Initialize(spriteCommon, kWhiteTexture)) {
		return false;
	}

	sprites_.push_back(std::move(clearSprite));
	sprites_.push_back(std::move(backSprite));
	// 白画面を最後に描き、Sceneに入った最初のFrameで見出しを隠します。
	sprites_.push_back(std::move(whiteSprite));
	Update(0.0f, static_cast<float>(directXCommon->GetClientWidth()),
		static_cast<float>(directXCommon->GetClientHeight()));
	return true;
}

// 見出しと案内の位置を保ち、白いSpriteだけを徐々に透明にして背景を見せます。
void StageClearOverlay::Update(float deltaTime, float clientWidth, float clientHeight)
{
	if (sprites_.size() != kSpriteCount || clientWidth <= 0.0f || clientHeight <= 0.0f) {
		return;
	}
	Engine::Sprite* clearSprite = sprites_[kClearSpriteIndex].get();
	const Vector2& clearTextureSize = clearSprite->GetTextureSize();
	const float clearWidth = (std::min)(
		clientWidth * 0.78f,
		clientHeight * 0.22f * clearTextureSize.x / clearTextureSize.y);
	const float clearHeight = clearWidth * clearTextureSize.y / clearTextureSize.x;
	clearSprite->SetPosition({ (clientWidth - clearWidth) * 0.5f, clientHeight * 0.16f });
	clearSprite->SetSize({ clearWidth, clearHeight });
	clearSprite->Update();

	Engine::Sprite* backSprite = sprites_[kBackSpriteIndex].get();
	const Vector2& backTextureSize = backSprite->GetTextureSize();
	const float backWidth = (std::min)(
		clientWidth * 0.70f,
		clientHeight * 0.16f * backTextureSize.x / backTextureSize.y);
	const float backHeight = backWidth * backTextureSize.y / backTextureSize.x;
	backSprite->SetPosition({
		(clientWidth - backWidth) * 0.5f,
		clientHeight * 0.75f - backHeight * 0.5f,
	});
	backSprite->SetSize({ backWidth, backHeight });
	backSprite->Update();

	// Scene初期化直後の重いFrameだけで白が消えないよう、一回分の進行を制限します。
	whiteFadeElapsed_ = (std::min)(
		whiteFadeElapsed_ + std::clamp(deltaTime, 0.0f, 0.05f), kWhiteFadeDuration);
	Engine::Sprite* whiteSprite = sprites_[kWhiteSpriteIndex].get();
	whiteSprite->SetPosition({ 0.0f, 0.0f });
	whiteSprite->SetSize({ clientWidth, clientHeight });
	whiteSprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f - whiteFadeElapsed_ / kWhiteFadeDuration });
	whiteSprite->Update();
}

// クリア画面の専用Spriteを解放し、次の初期化に古い画像を残しません。
void StageClearOverlay::Finalize()
{
	sprites_.clear();
	whiteFadeElapsed_ = 0.0f;
}

// Pipelineが描画する間だけ、所有するSprite一覧を参照させます。
const std::vector<std::unique_ptr<Engine::Sprite>>& StageClearOverlay::GetSprites() const
{
	return sprites_;
}
