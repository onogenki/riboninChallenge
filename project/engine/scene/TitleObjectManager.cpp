#include "TitleObjectManager.h"

#include "Object3d.h"
#include "Object3dFactory.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "DirectXCommon.h"
#include <algorithm>
#include <utility>
#include <array>
#include <numbers>

// 前方宣言した所有型を、完全な型が分かる場所で生成します。
TitleObjectManager::TitleObjectManager() = default;

// 所有するモデル・Spriteを、完全な型が分かる場所で破棄します。
TitleObjectManager::~TitleObjectManager() = default;

// 従来の一括初期化経路では、モデルとSpriteの両方を準備します。
bool TitleObjectManager::Initialize(
	Object3dCommon* object3dCommon,
	SpriteCommon* spriteCommon,
	DirectXCommon* directXCommon)
{
	return InitializeModel(object3dCommon) && InitializeSprites(spriteCommon, directXCommon);
}

// 導入映像の板と視点を下げた後の仮床を作り、Editorの初期配置件数を記録します。
bool TitleObjectManager::InitializeModel(Object3dCommon* object3dCommon)
{
	auto monitorScreen = Object3dFactory::Create(object3dCommon, "debug/plane.obj");
	auto floor = Object3dFactory::Create(object3dCommon, "debug/plane.obj");
	if (!monitorScreen || !floor ||
		!TextureManager::GetInstance()->LoadTexture("Resources/debug/grass.png")) {
		return false;
	}
	// Spriteを隠した後も同じ絵が見えるよう、正面に仮の監視画面を置きます。
	monitorScreen->SetTranslate({ 0.0f, 0.0f, 0.0f });
	// モデル読込時にXが反転するため、正面をCameraへ向けてSpriteと同じ左右にします。
	monitorScreen->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
	monitorScreen->SetScale({ 4.0f, 2.25f, 1.0f });
	monitorScreen->SetTextureOverride("Resources/Title/2dTitle/2DTitle1.png");
	// 遠くなったCameraから下を向いても見える位置へ、仮床だけ移します。
	floor->SetTranslate({ 0.0f, -3.0f, -40.0f });
	floor->SetRotate({ -std::numbers::pi_v<float> / 2.0f, 0.0f, 0.0f });
	floor->SetScale({ 20.0f, 20.0f, 1.0f });
	floor->SetTextureOverride("Resources/debug/grass.png");
	normalObjects_.push_back(std::move(monitorScreen));
	normalObjects_.push_back(std::move(floor));
	baseNormalObjectCount_ = normalObjects_.size();
	baseAnimationObjectCount_ = animationObjects_.size();
	return true;
}

// 白背景を下、仮画像を上の順で作り、モデルの準備前から描画できるようにします。
bool TitleObjectManager::InitializeSprites(SpriteCommon* spriteCommon, DirectXCommon* directXCommon)
{
	if (!spriteCommon || !directXCommon) {
		return false;
	}
	spriteCommon->Initialize(directXCommon);
	// どちらかの画像が欠けた場合は、半端なSpriteを一覧へ追加しません。
	if (!TextureManager::GetInstance()->LoadTexture("Resources/debug/white.png") ||
		!TextureManager::GetInstance()->LoadTexture("Resources/Title/2dTitle/2DTitle1.png")) {
		return false;
	}
	auto backgroundSprite = std::make_unique<Sprite>();
	backgroundSprite->Initialize(spriteCommon, "Resources/debug/white.png");
	backgroundSprite->SetPosition({ 0.0f, 0.0f });
	backgroundSprite->SetAnchorPoint({ 0.0f, 0.0f });
	sprites_.push_back(std::move(backgroundSprite));

	auto titleSprite = std::make_unique<Sprite>();
	titleSprite->Initialize(spriteCommon, "Resources/Title/2dTitle/2DTitle1.png");
	titleSprite->SetPosition({ 0.0f, 0.0f });
	titleSprite->SetAnchorPoint({ 0.0f, 0.0f });
	sprites_.push_back(std::move(titleSprite));
	ResizeTitleSprites();
	baseSpriteCount_ = sprites_.size();
	return true;
}

// Spriteの画像を切り替えた後も、白背景と仮画像を現在の画面サイズへ合わせます。
void TitleObjectManager::ResizeTitleSprites()
{
	if (sprites_.size() < 2) {
		return;
	}
	const DirectXCommon* directXCommon = DirectXCommon::GetInstance();
	const Vector2 screenSize{
		static_cast<float>(directXCommon->GetClientWidth()),
		static_cast<float>(directXCommon->GetClientHeight()),
	};
	sprites_[0]->SetSize(screenSize);
	sprites_[1]->SetSize(screenSize);
}

// 読み込みに成功した画像を、導入Spriteと仮の3D監視画面へ同時に設定します。
bool TitleObjectManager::SetTitleFrame(std::size_t frameIndex)
{
	static constexpr std::array<const char*, kTitleFrameCount> kFramePaths =
	{
		"Resources/Title/2dTitle/2DTitle1.png",
		"Resources/Title/2dTitle/2DTitle2.png",
		"Resources/Title/2dTitle/2DTitle3.png",
		"Resources/Title/2dTitle/2DTitle4.png",
		"Resources/Title/2dTitle/2DTitle5.png",
		"Resources/Title/2dTitle/2DTitle6.png",
		"Resources/Title/2dTitle/2DTitle7.png",
		"Resources/Title/2dTitle/2DTitle8.png",
		"Resources/Title/2dTitle/2DTitle9.png",
		"Resources/Title/2dTitle/2DTitle10.png",
		"Resources/Title/2dTitle/2DTitle11.png",
		"Resources/Title/2dTitle/2DTitle12.png",
	};

	if (sprites_.size() < 2 || frameIndex >= kFramePaths.size())
	{
		return false;
	}

	const std::string texturePath = kFramePaths[frameIndex];
	if (!TextureManager::GetInstance()->LoadTexture(texturePath))
	{
		return false;
	}

	Sprite* titleSprite = sprites_[1].get();
	titleSprite->SetTexture(texturePath);
	if (!normalObjects_.empty() && normalObjects_[0]) {
		normalObjects_[0]->SetTextureOverride(texturePath);
	}
	ResizeTitleSprites();
	return true;
}

// Shelfから選ばれたモデルを作り、通常モデルまたはAnimationモデルの一覧へ追加します。
bool TitleObjectManager::AddModel(Object3dCommon* object3dCommon, const std::string& fileName)
{
	auto object = Object3dFactory::Create(object3dCommon, fileName, true);
	if (!object) {
		return false;
	}
	// 追加順にX方向へずらし、同じ場所にモデルが重ならない従来の配置を保ちます。
	const float offset = static_cast<float>(normalObjects_.size() + animationObjects_.size()) * 1.4f;
	object->SetTranslate({ -2.0f + offset, 0.0f, 6.0f });
	object->SetScale({ 1.0f, 1.0f, 1.0f });
	if (object->IsSkeletal()) {
		Object3dFactory::LoadAndPlayAnimation(*object, fileName);
		animationObjects_.push_back(std::move(object));
	} else {
		normalObjects_.push_back(std::move(object));
	}
	return true;
}

// Shelfで追加した画像だけは縦横比を保った仮サイズにし、画像を引き伸ばさないようにします。
bool TitleObjectManager::AddTexture(SpriteCommon* spriteCommon, const std::string& textureFilePath)
{
	// 寸法取得前に読込結果を確認し、欠損画像でGetMetaDataを呼ばないようにします。
	if (!spriteCommon || !TextureManager::GetInstance()->LoadTexture(textureFilePath)) {
		return false;
	}
	const auto& metadata = TextureManager::GetInstance()->GetMetaData(textureFilePath);
	Vector2 size{ static_cast<float>(metadata.width), static_cast<float>(metadata.height) };
	const float largestSide = (std::max)(size.x, size.y);
	if (largestSide > 180.0f) {
		const float scale = 180.0f / largestSide;
		size = { size.x * scale, size.y * scale };
	}
	return AddTexture(spriteCommon, textureFilePath, { 180.0f, 160.0f }, size);
}

// 指定したピクセル位置・サイズで一度だけ配置し、以後のInspector編集を上書きしません。
bool TitleObjectManager::AddTexture(
	SpriteCommon* spriteCommon, const std::string& textureFilePath,
	const Vector2& position, const Vector2& size)
{
	// 直接配置の経路でも同じ読込確認を行い、失敗した画像を一覧に残しません。
	if (!spriteCommon || !TextureManager::GetInstance()->LoadTexture(textureFilePath)) {
		return false;
	}
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(spriteCommon, textureFilePath);
	sprite->SetAnchorPoint({ 0.5f, 0.5f });
	sprite->SetSize(size);
	sprite->SetPosition(position);
	sprites_.push_back(std::move(sprite));
	return true;
}

// 初期配置の件数より後ろだけを解放し、背景と初期画像を残します。
void TitleObjectManager::ClearAdded()
{
	if (normalObjects_.size() > baseNormalObjectCount_) {
		normalObjects_.resize(baseNormalObjectCount_);
	}
	if (animationObjects_.size() > baseAnimationObjectCount_) {
		animationObjects_.resize(baseAnimationObjectCount_);
	}
	if (sprites_.size() > baseSpriteCount_) {
		sprites_.resize(baseSpriteCount_);
	}
}

// GPU完了後にSceneから呼び、所有物と初期配置の境界をリセットします。
void TitleObjectManager::Finalize()
{
	sprites_.clear();
	normalObjects_.clear();
	animationObjects_.clear();
	baseNormalObjectCount_ = 0;
	baseAnimationObjectCount_ = 0;
	baseSpriteCount_ = 0;
}
