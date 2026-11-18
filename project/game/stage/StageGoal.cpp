#include "StageGoal.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "Player.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>

namespace
{
	constexpr const char* kWhiteTexture = "resources/debug/white.png";
	constexpr float kLightHeight = 8.0f;
	constexpr float kClearDuration = 1.65f;
	constexpr float kOuterLightWidth = 2.4f;
	constexpr float kInnerLightWidth = 0.85f;

	// 光の表示幅と接触判定で同じ水平倍率を使い、モデルの拡大に追従させます。
	float GetHorizontalScale(const Vector3& scale)
	{
		return (std::abs(scale.x) + std::abs(scale.z)) * 0.5f;
	}
}

StageGoal::StageGoal() = default;
StageGoal::~StageGoal() = default;

// 既存のLaser描画器を白い二層の光として使い、白画面のSpriteも先に準備します。
bool StageGoal::Initialize(Engine::DirectXCommon* directXCommon, Engine::SpriteCommon* spriteCommon)
{
	if (!directXCommon || !spriteCommon || !spriteCommon->Initialize(directXCommon) ||
		!outerLight_.Initialize(directXCommon, 1) || !innerLight_.Initialize(directXCommon, 1) ||
		!Engine::TextureManager::GetInstance()->LoadTexture(kWhiteTexture)) {
		return false;
	}
	outerLight_.SetBeamWidth(kOuterLightWidth);
	outerLight_.SetColor({ 0.85f, 0.90f, 1.0f, 0.40f });
	innerLight_.SetBeamWidth(kInnerLightWidth);
	innerLight_.SetColor({ 1.0f, 1.0f, 1.0f, 0.95f });
	auto overlay = std::make_unique<Engine::Sprite>();
	if (!overlay->Initialize(spriteCommon, kWhiteTexture)) {
		return false;
	}
	overlay->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
	overlaySprites_.push_back(std::move(overlay));
	return true;
}

// モデルのくぼみと倍率に光柱を合わせ、Editorや再読込の位置変更にも追従します。
void StageGoal::SetGoalTransform(const Transform& transform)
{
	goalCenter_ = transform.translate;
	goalScale_ = transform.scale;
	const float horizontalScale = GetHorizontalScale(goalScale_);
	outerLight_.SetBeamWidth(kOuterLightWidth * horizontalScale);
	innerLight_.SetBeamWidth(kInnerLightWidth * horizontalScale);
	const Vector3 origin{ goalCenter_.x, goalCenter_.y - 0.02f * goalScale_.y, goalCenter_.z };
	lightSegments_ = { { origin, { origin.x, origin.y + kLightHeight, origin.z } } };
	hasGoal_ = true;
}

void StageGoal::ClearGoal()
{
	hasGoal_ = false;
	isPlaying_ = false;
	lightSegments_.clear();
}

// Goalモデルの外縁、上の縁、中央のくぼみを半径別の高さとして扱い、ジャンプなしで登れるようにします。
void StageGoal::ApplyWalkableSurface(Player& player) const
{
	if (!hasGoal_ || isPlaying_) {
		return;
	}
	const Vector3 position = player.GetPosition();
	if (std::abs(goalScale_.x) < 0.01f || std::abs(goalScale_.z) < 0.01f) {
		return;
	}
	const float x = (position.x - goalCenter_.x) / goalScale_.x;
	const float z = (position.z - goalCenter_.z) / goalScale_.z;
	const float radius = std::sqrt(x * x + z * z);
	if (radius >= 1.65f) {
		return;
	}
	float localHeight = -0.06f;
	if (radius < 0.90f) {
		localHeight += 0.30f * radius / 0.90f;
	} else {
		localHeight = 0.24f - 0.79f * (radius - 0.90f) / 0.75f;
	}
	player.ApplyWalkableSurface(goalCenter_.y + localHeight * goalScale_.y);
}

// 台座全体ではなく、モデルの倍率に合わせた中央の光だけをGoalと判定します。
bool StageGoal::IsPlayerInLight(const Player& player) const
{
	if (!hasGoal_ || isPlaying_) {
		return false;
	}
	const Vector3& position = player.GetPosition();
	const float dx = position.x - goalCenter_.x;
	const float dz = position.z - goalCenter_.z;
	const float horizontalScale = GetHorizontalScale(goalScale_);
	const float lightRadius = 0.70f * horizontalScale;
	return dx * dx + dz * dz <= lightRadius * lightRadius &&
		position.y >= goalCenter_.y && position.y <= goalCenter_.y + kLightHeight;
}

// 現在のCameraから光の手前へ寄せ、切替直後に視点を飛ばしません。
void StageGoal::Begin(const Player& player, const Engine::Camera& camera)
{
	if (isPlaying_) {
		return;
	}
	isPlaying_ = true;
	elapsedTime_ = 0.0f;
	overlayAlpha_ = 0.0f;
	playerStart_ = player.GetPosition();
	cameraStart_ = camera.GetTranslate();
	// 光へ近づく距離を六割で止め、Playerと台座が見える余地を残します。
	cameraTarget_ = {
		cameraStart_.x + (goalCenter_.x - cameraStart_.x) * 0.60f,
		cameraStart_.y + (goalCenter_.y + 2.1f - cameraStart_.y) * 0.60f,
		cameraStart_.z + (goalCenter_.z - cameraStart_.z) * 0.60f,
	};
}

// 光の中心で球が上昇・透明化する間にCameraを接近させ、最後に画面全体を白くします。
bool StageGoal::Update(Player& player, Engine::Camera& camera, float deltaTime)
{
	if (!isPlaying_) {
		return false;
	}
	elapsedTime_ = (std::min)(elapsedTime_ + deltaTime, kClearDuration);
	UpdatePlayerPresentation(player);
	UpdateCameraPresentation(player, camera);
	UpdateOverlayPresentation();
	return elapsedTime_ >= kClearDuration;
}

// 先にPlayerを上昇・透明化し、後続のCameraが更新後の位置を見るようにします。
void StageGoal::UpdatePlayerPresentation(Player& player)
{
	const float rise = std::clamp(elapsedTime_ / 1.80f, 0.0f, 1.0f);
	const float fade = std::clamp((elapsedTime_ - 0.25f) / 1.20f, 0.0f, 1.0f);
	player.SetPosition({ goalCenter_.x, playerStart_.y + 3.8f * rise, goalCenter_.z });
	player.SetVisualOpacity(1.0f - fade);
	player.SetVisualSpecularGlow(1.0f + 12.0f * fade, 10.0f * fade * (1.0f - fade));
}

// 更新済みPlayerへ視線を向け、Cameraが光へ近づく演出を進めます。
void StageGoal::UpdateCameraPresentation(const Player& player, Engine::Camera& camera)
{
	// 前半を速く進めて最後だけ減速し、Cameraが止まるフレームにSceneを切り替えます。
	const float cameraTime = std::clamp(elapsedTime_ / kClearDuration, 0.0f, 1.0f);
	const float approach = 1.0f - (1.0f - cameraTime) * (1.0f - cameraTime);
	camera.SetTranslate({
		cameraStart_.x + (cameraTarget_.x - cameraStart_.x) * approach,
		cameraStart_.y + (cameraTarget_.y - cameraStart_.y) * approach,
		cameraStart_.z + (cameraTarget_.z - cameraStart_.z) * approach,
	});
	// 上昇するPlayerへ視線を保ち、光柱の真下では角度が不安定にならないよう既存のYawを使います。
	const Vector3 cameraPosition = camera.GetTranslate();
	const Vector3 lookDirection{
		player.GetPosition().x - cameraPosition.x,
		player.GetPosition().y + 0.45f - cameraPosition.y,
		player.GetPosition().z - cameraPosition.z,
	};
	const float horizontalLength = std::sqrt(
		lookDirection.x * lookDirection.x + lookDirection.z * lookDirection.z);
	if (horizontalLength > 0.05f) {
		camera.SetRotate({
			-std::atan2(lookDirection.y, horizontalLength) - 0.08f,
			std::atan2(lookDirection.x, lookDirection.z),
			0.0f,
		});
	}
}

// Cameraの更新後に白いSpriteを進め、画面切替直前まで描画します。
void StageGoal::UpdateOverlayPresentation()
{
	overlayAlpha_ = std::clamp((elapsedTime_ - 0.70f) / 0.85f, 0.0f, 1.0f);
	if (!overlaySprites_.empty()) {
		Engine::Sprite& overlay = *overlaySprites_.front();
		overlay.SetColor({ 1.0f, 1.0f, 1.0f, overlayAlpha_ });
		const Engine::DirectXCommon* directXCommon = Engine::DirectXCommon::GetInstance();
		overlay.SetPosition({ 0.0f, 0.0f });
		overlay.SetSize({
			static_cast<float>(directXCommon->GetClientWidth()),
			static_cast<float>(directXCommon->GetClientHeight()),
		});
		overlay.Update();
	}
}

float StageGoal::GetShadowVisibility() const
{
	return isPlaying_ ? 1.0f - std::clamp(elapsedTime_ / 0.75f, 0.0f, 1.0f) : 1.0f;
}

// 既存SpotLight配列の一枠に白い光を加え、光柱の周囲のモデルも強く照らします。
Engine::Object3d::SpotLight StageGoal::GetSpotLight() const
{
	Engine::Object3d::SpotLight light{};
	if (!hasGoal_) {
		return light;
	}
	light.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	light.position = lightSegments_.front().start;
	light.direction = { 0.0f, 1.0f, 0.0f };
	light.intensity = 8.0f;
	light.distance = kLightHeight;
	light.decay = 0.75f;
	light.cosAngle = 0.82f;
	light.cosFalloffStart = 0.97f;
	return light;
}

// 台座など不透明な3Dを描いた後に、輪郭と芯の二層を加算合成で重ねます。
void StageGoal::DrawLight(const Engine::Camera& camera)
{
	if (!hasGoal_) {
		return;
	}
	outerLight_.Draw(lightSegments_, camera);
	innerLight_.Draw(lightSegments_, camera);
}
