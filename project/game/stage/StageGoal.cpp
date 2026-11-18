#include "StageGoal.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "ParticleManager.h"
#include "Player.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
	constexpr const char* kWhiteTexture = "resources/debug/white.png";
	constexpr const char* kGoalParticleGroup = "StageGoalLight";
	constexpr const char* kGoalParticleTexture = "Resources/debug/circle2.png";
	constexpr float kLightHeight = 8.0f;
	constexpr float kOrbitDuration = 1.40f;
	constexpr float kFrontMoveDuration = 0.30f;
	constexpr float kFrontHoldDuration = 0.18f;
	constexpr float kOrbitRadius = 2.10f;
	constexpr float kFrontRadius = 0.85f;
	constexpr float kCameraPlayerGap = 7.00f;
	constexpr float kCameraApproachDuration = kOrbitDuration + kFrontMoveDuration;
	constexpr float kSqueezeStart = kCameraApproachDuration + kFrontHoldDuration;
	constexpr float kSqueezeDuration = 0.38f;
	constexpr float kRiseStart = kSqueezeStart + kSqueezeDuration;
	constexpr float kRiseDuration = 0.22f;
	constexpr float kClearDuration = kRiseStart + 0.55f;
	constexpr float kSparkleInterval = 0.10f;
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
	// 一つの既存Groupに三種類の光を入れ、画像とSRVの登録失敗をStage開始前に検出します。
	return Engine::ParticleManager::GetInstance()->CreateParticleGroup(kGoalParticleGroup, kGoalParticleTexture);
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
	sparkleTimer_ = 0.0f;
	lightSegments_.clear();
	// Sceneを離れた後に別の画面へGoalの粒子を持ち越さないようにします。
	Engine::ParticleManager::GetInstance()->ClearParticles(kGoalParticleGroup);
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

// 現在位置から一周する軌道とCamera側の停止位置を作り、演出開始時の瞬間移動を防ぎます。
void StageGoal::Begin(const Player& player, const Engine::Camera& camera)
{
	if (isPlaying_) {
		return;
	}
	isPlaying_ = true;
	elapsedTime_ = 0.0f;
	sparkleTimer_ = 0.0f;
	overlayAlpha_ = 0.0f;
	Engine::ParticleManager::GetInstance()->ClearParticles(kGoalParticleGroup);
	playerStart_ = player.GetPosition();
	cameraStart_ = camera.GetTranslate();
	cameraStartRotation_ = camera.GetRotate();
	const float cameraDx = cameraStart_.x - goalCenter_.x;
	const float cameraDz = cameraStart_.z - goalCenter_.z;
	const float cameraDistance = std::sqrt(cameraDx * cameraDx + cameraDz * cameraDz);
	const Vector3 frontDirection = cameraDistance > 0.001f
		? Vector3{ cameraDx / cameraDistance, 0.0f, cameraDz / cameraDistance }
		: Vector3{ 0.0f, 0.0f, -1.0f };
	frontPosition_ = {
		goalCenter_.x + frontDirection.x * kFrontRadius,
		playerStart_.y,
		goalCenter_.z + frontDirection.z * kFrontRadius,
	};
	const float startDx = playerStart_.x - goalCenter_.x;
	const float startDz = playerStart_.z - goalCenter_.z;
	orbitStartRadius_ = std::sqrt(startDx * startDx + startDz * startDz);
	orbitStartDirection_ = orbitStartRadius_ > 0.001f
		? Vector3{ startDx / orbitStartRadius_, 0.0f, startDz / orbitStartRadius_ }
		: frontDirection;
	// 手前に立つPlayerの全身が見える距離で止め、細くなる演出中も近づき過ぎないようにします。
	const float targetDistance = (std::max)(cameraDistance * 0.65f, kFrontRadius + kCameraPlayerGap);
	cameraTarget_ = {
		goalCenter_.x + frontDirection.x * targetDistance,
		cameraStart_.y + (goalCenter_.y + 2.1f - cameraStart_.y) * 0.60f,
		goalCenter_.z + frontDirection.z * targetDistance,
	};
}

// 一周とCamera接近中の細い姿、停止直後の急上昇、白画面の順に進めます。
bool StageGoal::Update(Player& player, Engine::Camera& camera, float deltaTime)
{
	if (!isPlaying_) {
		return false;
	}
	const float previousElapsedTime = elapsedTime_;
	elapsedTime_ = (std::min)(elapsedTime_ + deltaTime, kClearDuration);
	UpdatePlayerPresentation(player);
	UpdateCameraPresentation(player, camera);
	UpdateParticles(player, previousElapsedTime, deltaTime);
	UpdateOverlayPresentation();
	return elapsedTime_ >= kClearDuration;
}

// 周回中の小光、細くなる時の光核、急上昇時の渦を既存の発生関数で順に出します。
void StageGoal::UpdateParticles(const Player& player, float previousElapsedTime, float deltaTime)
{
	Engine::ParticleManager* particles = Engine::ParticleManager::GetInstance();
	if (elapsedTime_ < kOrbitDuration) {
		sparkleTimer_ += (std::max)(deltaTime, 0.0f);
		if (sparkleTimer_ >= kSparkleInterval) {
			sparkleTimer_ -= kSparkleInterval;
			const Vector3& position = player.GetPosition();
			particles->EmitPillarSparkle(
				kGoalParticleGroup, 6, { position.x, position.y + 0.35f, position.z }, 0.75f);
		}
	}
	if (previousElapsedTime < kSqueezeStart && elapsedTime_ >= kSqueezeStart) {
		particles->EmitLightCore(
			kGoalParticleGroup, 12, { goalCenter_.x, playerStart_.y + 0.45f, goalCenter_.z }, 0.65f);
	}
	if (previousElapsedTime < kRiseStart && elapsedTime_ >= kRiseStart) {
		const Vector3& position = player.GetPosition();
		particles->EmitLightSpiral(
			kGoalParticleGroup, 28, { position.x, position.y + 0.35f, position.z }, 0.65f);
	}
}

// 最初はGoalの周囲を加速しながら回り、Camera側へ戻ってから細くなって上昇します。
void StageGoal::UpdatePlayerPresentation(Player& player)
{
	Vector3 position = playerStart_;
	Vector3 facingTarget = cameraStart_;
	const float squeezeTime = std::clamp((elapsedTime_ - kSqueezeStart) / kSqueezeDuration, 0.0f, 1.0f);
	const float squeeze = squeezeTime * squeezeTime * (3.0f - 2.0f * squeezeTime);
	const float orbitTime = std::clamp(elapsedTime_ / kOrbitDuration, 0.0f, 1.0f);
	const float startAngle = std::atan2(orbitStartDirection_.z, orbitStartDirection_.x);
	const float angle = startAngle - 2.0f * std::numbers::pi_v<float> * orbitTime * orbitTime;
	const float radiusBlend = std::clamp(orbitTime / 0.18f, 0.0f, 1.0f);
	const float radius = orbitStartRadius_ + (kOrbitRadius - orbitStartRadius_) * radiusBlend;
	const Vector3 orbitEnd{
		goalCenter_.x + orbitStartDirection_.x * kOrbitRadius,
		playerStart_.y,
		goalCenter_.z + orbitStartDirection_.z * kOrbitRadius,
	};
	if (elapsedTime_ < kOrbitDuration) {
		position = {
			goalCenter_.x + std::cos(angle) * radius,
			playerStart_.y + 0.20f * std::sin(4.0f * std::numbers::pi_v<float> * orbitTime),
			goalCenter_.z + std::sin(angle) * radius,
		};
		facingTarget = { position.x + std::sin(angle), position.y, position.z - std::cos(angle) };
	} else if (elapsedTime_ < kCameraApproachDuration) {
		const float moveTime = std::clamp((elapsedTime_ - kOrbitDuration) / kFrontMoveDuration, 0.0f, 1.0f);
		const float move = moveTime * moveTime * (3.0f - 2.0f * moveTime);
		position = {
			orbitEnd.x + (frontPosition_.x - orbitEnd.x) * move,
			playerStart_.y,
			orbitEnd.z + (frontPosition_.z - orbitEnd.z) * move,
		};
	} else {
		position = {
			frontPosition_.x + (goalCenter_.x - frontPosition_.x) * squeeze,
			playerStart_.y,
			frontPosition_.z + (goalCenter_.z - frontPosition_.z) * squeeze,
		};
	}
	const float horizontalScale = 1.0f - 0.78f * squeeze;
	player.SetVisualScale({ horizontalScale, 1.0f, horizontalScale });
	const float riseTime = std::clamp((elapsedTime_ - kRiseStart) / kRiseDuration, 0.0f, 1.0f);
	const float rise = 1.0f - std::pow(1.0f - riseTime, 3.0f);
	const float fade = std::clamp((elapsedTime_ - kRiseStart - 0.08f) / 0.34f, 0.0f, 1.0f);
	player.SetPosition({ position.x, position.y + 3.8f * rise, position.z });
	player.FaceToward(facingTarget);
	player.SetVisualOpacity(1.0f - fade);
	player.SetVisualSpecularGlow(1.0f + 12.0f * fade, 10.0f * fade * (1.0f - fade));
}

// 周回中は向きを保ち、停止後の上昇だけを縦回転で追うことでPlayerを画面内に残します。
void StageGoal::UpdateCameraPresentation(const Player& player, Engine::Camera& camera)
{
	// 体が細くなる間も接近を続け、Camera停止とPlayerの急上昇を同じ時刻に揃えます。
	const float cameraTime = std::clamp(elapsedTime_ / kRiseStart, 0.0f, 1.0f);
	const float approach = 1.0f - (1.0f - cameraTime) * (1.0f - cameraTime);
	camera.SetTranslate({
		cameraStart_.x + (cameraTarget_.x - cameraStart_.x) * approach,
		cameraStart_.y + (cameraTarget_.y - cameraStart_.y) * approach,
		cameraStart_.z + (cameraTarget_.z - cameraStart_.z) * approach,
	});
	if (elapsedTime_ < kRiseStart) {
		return;
	}
	// Goalの中心への水平距離は固定し、上昇した高さの差だけCameraを上へ向けます。
	const float dx = goalCenter_.x - cameraTarget_.x;
	const float dz = goalCenter_.z - cameraTarget_.z;
	const float horizontalDistance = (std::max)(std::sqrt(dx * dx + dz * dz), 0.05f);
	const float startHeight = playerStart_.y + 0.45f - cameraTarget_.y;
	const float currentHeight = player.GetPosition().y + 0.45f - cameraTarget_.y;
	const float pitchDelta = std::atan2(startHeight, horizontalDistance) -
		std::atan2(currentHeight, horizontalDistance);
	camera.SetRotate({
		cameraStartRotation_.x + pitchDelta,
		cameraStartRotation_.y,
		cameraStartRotation_.z,
	});
}

// 急上昇が始まってから白いSpriteを重ね、周回と細くなる動きを隠さないようにします。
void StageGoal::UpdateOverlayPresentation()
{
	overlayAlpha_ = std::clamp((elapsedTime_ - kRiseStart - 0.06f) / 0.45f, 0.0f, 1.0f);
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

// 細くなる間は床影を残し、実際に上昇し始めた時だけ影を消します。
float StageGoal::GetShadowVisibility() const
{
	return isPlaying_ ? 1.0f - std::clamp((elapsedTime_ - kRiseStart) / kRiseDuration, 0.0f, 1.0f) : 1.0f;
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
