#include "StageSelectOverlay.h"

#include "DirectXCommon.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <algorithm>
#include <utility>

namespace
{
	constexpr const char* kBackToTitleTexture = "resources/font/backSpaceTitle.png";
	constexpr const char* kStageSelectTexture = "resources/font/stageSelect.png";
	constexpr const char* kEnterTexture = "resources/font/ENTER.png";
	constexpr float kScreenMargin = 24.0f;
}

// 前方宣言したSpriteを、この実装ファイルで安全に生成します。
StageSelectOverlay::StageSelectOverlay() = default;

// 所有するSpriteを、完全な型を知る場所で解放します。
StageSelectOverlay::~StageSelectOverlay() = default;

// 背景とは別に三枚の画像を読み、すべてできた時だけSceneへ渡します。
bool StageSelectOverlay::Initialize(Engine::SpriteCommon* spriteCommon, Engine::DirectXCommon* directXCommon)
{
	sprites_.clear();
	if (!spriteCommon || !directXCommon || !spriteCommon->Initialize(directXCommon)) {
		return false;
	}
	Engine::TextureManager* textureManager = Engine::TextureManager::GetInstance();
	if (!textureManager->LoadTexture(kBackToTitleTexture) ||
		!textureManager->LoadTexture(kStageSelectTexture) ||
		!textureManager->LoadTexture(kEnterTexture)) {
		return false;
	}

	auto backSprite = std::make_unique<Engine::Sprite>();
	if (!backSprite->Initialize(spriteCommon, kBackToTitleTexture)) {
		return false;
	}
	backSprite->SetTextureLeftTop({ 467.0f, 848.0f });
	backSprite->SetTextureSize({ 1189.0f, 419.0f });

	auto stageSelectSprite = std::make_unique<Engine::Sprite>();
	if (!stageSelectSprite->Initialize(spriteCommon, kStageSelectTexture)) {
		return false;
	}
	stageSelectSprite->SetTextureLeftTop({ 17.0f, 973.0f });
	stageSelectSprite->SetTextureSize({ 1955.0f, 271.0f });

	auto enterSprite = std::make_unique<Engine::Sprite>();
	if (!enterSprite->Initialize(spriteCommon, kEnterTexture)) {
		return false;
	}
	enterSprite->SetTextureLeftTop({ 379.0f, 757.0f });
	enterSprite->SetTextureSize({ 1283.0f, 419.0f });

	sprites_.push_back(std::move(backSprite));
	sprites_.push_back(std::move(enterSprite));
	sprites_.push_back(std::move(stageSelectSprite));
	Update(static_cast<float>(directXCommon->GetClientWidth()),
		static_cast<float>(directXCommon->GetClientHeight()));
	return true;
}

// 切り抜き後の縦横比を保ち、中央の見出しを左上の案内と同じ高さに置きます。
void StageSelectOverlay::Update(float clientWidth, float clientHeight)
{
	if (sprites_.size() != 3 || clientWidth <= 0.0f || clientHeight <= 0.0f) {
		return;
	}

	Engine::Sprite* backSprite = sprites_[0].get();
	const Vector2& backTextureSize = backSprite->GetTextureSize();
	const float backWidth = (std::min)(
		clientWidth * 0.30f,
		clientHeight * 0.13f * backTextureSize.x / backTextureSize.y);
	const float backHeight = backWidth * backTextureSize.y / backTextureSize.x;
	backSprite->SetPosition({ kScreenMargin, kScreenMargin });
	backSprite->SetSize({ backWidth, backHeight });
	backSprite->Update();

	Engine::Sprite* stageSelectSprite = sprites_[2].get();
	const Vector2& stageSelectTextureSize = stageSelectSprite->GetTextureSize();
	const float stageSelectWidth = backHeight * stageSelectTextureSize.x / stageSelectTextureSize.y;
	stageSelectSprite->SetPosition({ (clientWidth - stageSelectWidth) * 0.5f, kScreenMargin });
	stageSelectSprite->SetSize({ stageSelectWidth, backHeight });
	stageSelectSprite->Update();

	Engine::Sprite* enterSprite = sprites_[1].get();
	const Vector2& enterTextureSize = enterSprite->GetTextureSize();
	const float enterWidth = (std::min)(
		clientWidth * 0.42f,
		clientHeight * 0.16f * enterTextureSize.x / enterTextureSize.y);
	const float enterHeight = enterWidth * enterTextureSize.y / enterTextureSize.x;
	enterSprite->SetPosition({ (clientWidth - enterWidth) * 0.5f, clientHeight - enterHeight - kScreenMargin });
	enterSprite->SetSize({ enterWidth, enterHeight });
	enterSprite->Update();
}

// Sceneの切替後に、案内SpriteのGPU資源を通常の所有関係で解放します。
void StageSelectOverlay::Finalize()
{
	sprites_.clear();
}

// SceneRenderPipelineが三枚を順に重ねられるよう、読み取り用一覧を返します。
const std::vector<std::unique_ptr<Engine::Sprite>>& StageSelectOverlay::GetSprites() const
{
	return sprites_;
}
