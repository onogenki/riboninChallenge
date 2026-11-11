#include "StagePauseOverlay.h"

#include "Camera.h"
#include "CarryableMirror.h"
#include "DirectXCommon.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	constexpr const char* kTabTexture = "resources/font/tab.png";
	constexpr const char* kPauseTexture = "resources/font/pause.png";
	constexpr const char* kBackToStageSelectTexture = "resources/font/backSpaceStageSelect.png";
	constexpr const char* kPickupTexture = "resources/font/E.png";
	constexpr float kScreenMargin = 24.0f;
}

// 前方宣言したSpriteを、この実装ファイルで安全に生成します。
StagePauseOverlay::StagePauseOverlay() = default;

// 所有するSpriteを、完全な型を知る場所で解放します。
StagePauseOverlay::~StagePauseOverlay() = default;

// 本編開始前に四枚を一度だけ読み、欠けた案内を表示しないようにします。
bool StagePauseOverlay::Initialize(Engine::SpriteCommon* spriteCommon, Engine::DirectXCommon* directXCommon)
{
	Finalize();
	if (!spriteCommon || !directXCommon || !spriteCommon->Initialize(directXCommon)) {
		return false;
	}
	Engine::TextureManager* textureManager = Engine::TextureManager::GetInstance();
	if (!textureManager->LoadTexture(kTabTexture) ||
		!textureManager->LoadTexture(kPauseTexture) ||
		!textureManager->LoadTexture(kBackToStageSelectTexture) ||
		!textureManager->LoadTexture(kPickupTexture)) {
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
	// 画像の寸法を変えても古い切り抜き座標で欠けないよう、E.png全体を使います。
	auto pickupSprite = std::make_unique<Engine::Sprite>();
	if (!pickupSprite->Initialize(spriteCommon, kPickupTexture)) {
		return false;
	}
	pickupSprite->SetAnchorPoint({ 0.5f, 0.75f });

	tabSprites_.push_back(std::move(tabSprite));
	pauseSprites_.push_back(std::move(pauseSprite));
	pauseSprites_.push_back(std::move(backSprite));
	pickupSprites_.push_back(std::move(pickupSprite));
	Update(static_cast<float>(directXCommon->GetClientWidth()),
		static_cast<float>(directXCommon->GetClientHeight()));
	return true;
}

// Camera更新後の鏡位置を画面へ投影し、E操作と同じ距離で案内を出します。
void StagePauseOverlay::UpdatePickupPrompt(
	const Engine::Camera* camera,
	const CarryableMirror* mirror,
	const Vector3& playerPosition,
	bool enabled,
	float clientWidth,
	float clientHeight)
{
	isPickupPromptVisible_ = false;
	if (!enabled || !camera || !mirror || pickupSprites_.size() != 1 ||
		clientWidth <= 0.0f || clientHeight <= 0.0f ||
		!mirror->CanBePickedUp(playerPosition)) {
		return;
	}

	const Vector3& center = mirror->GetMirror().GetCenter();
	const Vector3 promptPosition{
		center.x,
		center.y + mirror->GetMirror().GetHeight() * 0.5f + 0.45f,
		center.z,
	};
	const Matrix4x4& viewProjection = camera->GetViewProjectionMatrix();
	const float clipX = promptPosition.x * viewProjection.m[0][0] +
		promptPosition.y * viewProjection.m[1][0] + promptPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
	const float clipY = promptPosition.x * viewProjection.m[0][1] +
		promptPosition.y * viewProjection.m[1][1] + promptPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
	const float clipZ = promptPosition.x * viewProjection.m[0][2] +
		promptPosition.y * viewProjection.m[1][2] + promptPosition.z * viewProjection.m[2][2] + viewProjection.m[3][2];
	const float clipW = promptPosition.x * viewProjection.m[0][3] +
		promptPosition.y * viewProjection.m[1][3] + promptPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];
	if (!std::isfinite(clipX) || !std::isfinite(clipY) || !std::isfinite(clipZ) ||
		!std::isfinite(clipW) || clipW <= 0.0f || clipZ < 0.0f || clipZ > clipW) {
		return;
	}
	const float screenX = (clipX / clipW + 1.0f) * 0.5f * clientWidth;
	const float screenY = (1.0f - clipY / clipW) * 0.5f * clientHeight;
	if (!std::isfinite(screenX) || !std::isfinite(screenY) ||
		screenX < 0.0f || screenX > clientWidth || screenY < 0.0f || screenY > clientHeight) {
		return;
	}

	Engine::Sprite* sprite = pickupSprites_[0].get();
	// 透明余白を含む画像全体の比率を保ち、E案内を鏡の上へ配置します。
	const float iconHeight = (std::min)(clientHeight * 0.16f, clientWidth * 0.15f);
	const Vector2& textureSize = sprite->GetTextureSize();
	sprite->SetPosition({ screenX, screenY });
	sprite->SetSize({ iconHeight * textureSize.x / textureSize.y, iconHeight });
	sprite->Update();
	isPickupPromptVisible_ = true;
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

// GPU完了を待つScene終了時に、常時・停止中・拾う時の案内をまとめて解放します。
void StagePauseOverlay::Finalize()
{
	tabSprites_.clear();
	pauseSprites_.clear();
	pickupSprites_.clear();
	isPickupPromptVisible_ = false;
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

// E案内のSpriteは所有したまま、表示が必要なFrameだけ描画側へ貸します。
const std::vector<std::unique_ptr<Engine::Sprite>>& StagePauseOverlay::GetPickupSprites() const
{
	return pickupSprites_;
}
