#include "ParticleManager.h"

#include "CameraManager.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace MyMath;

// Hit、Ring、Lightなど、既成パーティクル演出の発生処理をまとめる。
// 粒子の更新・GPU描画とは分け、演出を追加する時に探しやすくする。
void ParticleManager::EmitHitEffect(const std::string name, uint32_t count, const Vector3& translate, float scaleMultiplier) {
	if (returnState_.IsReturning()) return;
    auto groupIt = particleGroups_.find(name);
    if (groupIt == particleGroups_.end()) {
        return;
    }
    ParticleGroup& group = groupIt->second;

    std::uniform_real_distribution<float> distRotate(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> distScale(0.4f, 1.5f);
    std::uniform_real_distribution<float> distTime(0.15f, 0.35f);

    for (uint32_t i = 0; i < count; ++i) {
        Particle newParticle;
        newParticle.transform.scale = { 0.05f * scaleMultiplier, distScale(randomEngine_) * scaleMultiplier, 1.0f };
        newParticle.transform.rotate = { 0.0f, 0.0f, distRotate(randomEngine_) };
        newParticle.transform.translate = translate;
        newParticle.velocity = { 0.0f, 0.0f, 0.0f };
        newParticle.color = { 1.0f, 1.0f, 1.0f, 1.0f };
        newParticle.lifeTime = distTime(randomEngine_);
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;

        group.particles.push_back(newParticle);
    }
}

//インパクト(円)エフェクト発生
void ParticleManager::EmitRingEffect(const std::string name, uint32_t count, const Vector3& translate, float scaleMultiplier)
{
	if (returnState_.IsReturning()) return;
    auto groupIt = particleGroups_.find(name);
    if (groupIt == particleGroups_.end()) {
        return;
    }
    ParticleGroup& group = groupIt->second;
    std::uniform_real_distribution<float> distRotate(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);
    std::uniform_real_distribution<float> distScale(1.0f, 1.8f);
    std::uniform_real_distribution<float> distLifeTime(0.3f, 0.6f);

    for (uint32_t i = 0; i < count; ++i) {
        Particle newParticle;
        float scale = distScale(randomEngine_);
        newParticle.transform.scale = { scale * scaleMultiplier, scale * scaleMultiplier, 1.0f };
        newParticle.transform.rotate = { distRotate(randomEngine_), distRotate(randomEngine_), distRotate(randomEngine_) };
        newParticle.transform.translate = translate;
        newParticle.velocity = { 0.0f, 0.0f, 0.0f };
        newParticle.color = { 1.0f, 1.0f, 1.0f, 0.8f };
        newParticle.lifeTime = distLifeTime(randomEngine_);
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;

        group.particles.push_back(newParticle);
    }
}

//ポータル(円柱)エフェクト発生
void ParticleManager::EmitCylinderEffect(const std::string name, uint32_t count, const Vector3& translate, float scaleMultiplier)
{
	if (returnState_.IsReturning()) return;
    auto groupIt = particleGroups_.find(name);
    if (groupIt == particleGroups_.end()) {
        return;
    }
    ParticleGroup& group = groupIt->second;
    std::uniform_real_distribution<float> distHeight(0.5f, 1.2f);
    std::uniform_real_distribution<float> distStretchSpeed(0.5f, 0.7f);

    for (uint32_t i = 0; i < count; ++i) {
        Particle newParticle;
        newParticle.transform.scale = { 0.75f * scaleMultiplier, distHeight(randomEngine_) * scaleMultiplier, 0.75f * scaleMultiplier };
        newParticle.transform.rotate = { 0.0f, 0.0f, 0.0f };
        newParticle.transform.translate = translate;
        newParticle.velocity = { 0.0f, 0.0f, 0.0f };
        newParticle.color = { 0.6f, 0.8f, 1.0f, 0.8f };
        newParticle.lifeTime = 0.0f;
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;
        newParticle.isEndless = true;
        newParticle.scaleVelocityY = distStretchSpeed(randomEngine_);

        group.particles.push_back(newParticle);
    }
}

void ParticleManager::EmitPillarSparkle(const std::string name, uint32_t count, const Vector3& position, float scaleMultiplier)
{
	if (returnState_.IsReturning()) return;
    auto groupIt = particleGroups_.find(name);
    if (groupIt == particleGroups_.end()) {
        return;
    }
    ParticleGroup& group = groupIt->second;
    std::uniform_real_distribution<float> rightOffsetDistribution(-0.45f, 0.45f);
    std::uniform_real_distribution<float> upOffsetDistribution(-0.1f, 1.25f);
    std::uniform_real_distribution<float> sideSpeedDistribution(-0.18f, 0.18f);
    std::uniform_real_distribution<float> upSpeedDistribution(0.4f, 1.1f);
    std::uniform_real_distribution<float> scaleDistribution(0.025f, 0.07f);
    std::uniform_real_distribution<float> lifeTimeDistribution(0.35f, 0.9f);
    std::uniform_real_distribution<float> rotateDistribution(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);

    Vector3 cameraRight = { 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };
    if (cameraManager_ && cameraManager_->GetActiveCamera()) {
        const Matrix4x4& cameraWorld = cameraManager_->GetActiveCamera()->GetWorldMatrix();
        cameraRight = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
        cameraUp = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
    }

    for (uint32_t index = 0; index < count; ++index) {
        Particle newParticle{};
        const float offsetRight = rightOffsetDistribution(randomEngine_);
        const float offsetUp = upOffsetDistribution(randomEngine_);
        const float sideSpeed = sideSpeedDistribution(randomEngine_);
        const float upSpeed = upSpeedDistribution(randomEngine_);
        const float startScale = scaleDistribution(randomEngine_) * scaleMultiplier;

        newParticle.transform.translate = {
            position.x + cameraRight.x * offsetRight + cameraUp.x * offsetUp,
            position.y + cameraRight.y * offsetRight + cameraUp.y * offsetUp,
            position.z + cameraRight.z * offsetRight + cameraUp.z * offsetUp
        };
        newParticle.velocity = {
            cameraRight.x * sideSpeed + cameraUp.x * upSpeed,
            cameraRight.y * sideSpeed + cameraUp.y * upSpeed,
            cameraRight.z * sideSpeed + cameraUp.z * upSpeed
        };
        newParticle.transform.rotate.z = rotateDistribution(randomEngine_);

        newParticle.useColorAndScaleOverLife = true;
        newParticle.startScale = { startScale, startScale, startScale };
        newParticle.endScale = { startScale * 0.2f, startScale * 0.2f, startScale * 0.2f };
        newParticle.startColor = { 1.0f, 1.0f, 0.92f, 1.0f };
        newParticle.endColor = { 1.0f, 0.62f, 0.08f, 0.0f };
        newParticle.transform.scale = newParticle.startScale;
        newParticle.color = newParticle.startColor;
        newParticle.lifeTime = lifeTimeDistribution(randomEngine_);
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;
        group.particles.push_back(newParticle);
    }
}

void ParticleManager::EmitLightCore(const std::string name, uint32_t count, const Vector3& position, float scaleMultiplier)
{
	if (returnState_.IsReturning()) return;
    auto groupIt = particleGroups_.find(name);
    if (groupIt == particleGroups_.end()) {
        return;
    }
    ParticleGroup& group = groupIt->second;
    std::uniform_real_distribution<float> offsetDistribution(-0.08f, 0.08f);
    Vector3 cameraRight = { 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };
    if (cameraManager_ && cameraManager_->GetActiveCamera()) {
        const Matrix4x4& cameraWorld = cameraManager_->GetActiveCamera()->GetWorldMatrix();
        cameraRight = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
        cameraUp = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
    }

    for (uint32_t index = 0; index < count; ++index) {
        Particle newParticle{};
        const float offsetRight = offsetDistribution(randomEngine_);
        const float offsetUp = offsetDistribution(randomEngine_);
        newParticle.transform.translate = {
            position.x + cameraRight.x * offsetRight + cameraUp.x * offsetUp,
            position.y + cameraRight.y * offsetRight + cameraUp.y * offsetUp,
            position.z + cameraRight.z * offsetRight + cameraUp.z * offsetUp
        };
        newParticle.velocity = { 0.0f, 0.0f, 0.0f };
        newParticle.useColorAndScaleOverLife = true;
        newParticle.startScale = { 0.08f * scaleMultiplier, 0.08f * scaleMultiplier, 0.08f * scaleMultiplier };
        newParticle.endScale = { 0.9f * scaleMultiplier, 0.9f * scaleMultiplier, 0.9f * scaleMultiplier };
        newParticle.startColor = { 1.0f, 1.0f, 0.92f, 1.0f };
        newParticle.endColor = { 1.0f, 0.65f, 0.12f, 0.0f };
        newParticle.transform.scale = newParticle.startScale;
        newParticle.color = newParticle.startColor;
        newParticle.lifeTime = 1.2f;
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;
        group.particles.push_back(newParticle);
    }
}

void ParticleManager::EmitLightRain(const std::string name, uint32_t count, const Vector3& position, float scaleMultiplier)
{
	if (returnState_.IsReturning()) return;
    auto groupIt = particleGroups_.find(name);
    if (groupIt == particleGroups_.end()) {
        return;
    }
    ParticleGroup& group = groupIt->second;
    std::uniform_real_distribution<float> positionXDistribution(-1.7f, 1.7f);
    std::uniform_real_distribution<float> positionYDistribution(1.8f, 3.5f);
    std::uniform_real_distribution<float> speedDistribution(-4.0f, -2.5f);
    std::uniform_real_distribution<float> lifeTimeDistribution(1.2f, 2.2f);

    Vector3 cameraRight = { 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };
    if (cameraManager_ && cameraManager_->GetActiveCamera()) {
        const Matrix4x4& cameraWorld = cameraManager_->GetActiveCamera()->GetWorldMatrix();
        cameraRight = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
        cameraUp = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
    }

    for (uint32_t index = 0; index < count; ++index) {
        Particle newParticle{};
        const float offsetRight = positionXDistribution(randomEngine_);
        const float offsetUp = positionYDistribution(randomEngine_);
        const float fallSpeed = speedDistribution(randomEngine_);
        newParticle.transform.translate = {
            position.x + cameraRight.x * offsetRight + cameraUp.x * offsetUp,
            position.y + cameraRight.y * offsetRight + cameraUp.y * offsetUp,
            position.z + cameraRight.z * offsetRight + cameraUp.z * offsetUp
        };
        newParticle.velocity = {
            cameraUp.x * fallSpeed,
            cameraUp.y * fallSpeed,
            cameraUp.z * fallSpeed
        };
        newParticle.useColorAndScaleOverLife = true;
        newParticle.startScale = { 0.035f * scaleMultiplier, 0.45f * scaleMultiplier, 0.035f * scaleMultiplier };
        newParticle.endScale = { 0.01f * scaleMultiplier, 0.15f * scaleMultiplier, 0.01f * scaleMultiplier };
        newParticle.startColor = { 1.0f, 0.98f, 0.72f, 0.9f };
        newParticle.endColor = { 1.0f, 0.55f, 0.05f, 0.0f };
        newParticle.transform.scale = newParticle.startScale;
        newParticle.color = newParticle.startColor;
        newParticle.lifeTime = lifeTimeDistribution(randomEngine_);
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;
        group.particles.push_back(newParticle);
    }
}

void ParticleManager::EmitLightSpiral(const std::string name, uint32_t count, const Vector3& translate, float scaleMultiplier)
{
	if (returnState_.IsReturning()) return;
    auto groupIt = particleGroups_.find(name);
    if (groupIt == particleGroups_.end()) {
        return;
    }
    ParticleGroup& group = groupIt->second;
    const uint32_t particlesPerArm = (std::max)(count / 2, 1u);
    const float minimumRadius = 0.2f * scaleMultiplier;
    const float maximumRadius = 1.4f * scaleMultiplier;
    const float twoRotations = std::numbers::pi_v<float> * 4.0f;

    Vector3 cameraRight = { 1.0f, 0.0f, 0.0f };
    Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };
    if (cameraManager_ && cameraManager_->GetActiveCamera()) {
        const Matrix4x4& cameraWorld = cameraManager_->GetActiveCamera()->GetWorldMatrix();
        cameraRight = Normalize({ cameraWorld.m[0][0], cameraWorld.m[0][1], cameraWorld.m[0][2] });
        cameraUp = Normalize({ cameraWorld.m[1][0], cameraWorld.m[1][1], cameraWorld.m[1][2] });
    }

    for (uint32_t index = 0; index < count; ++index) {
        Particle newParticle{};
        const uint32_t armIndex = index / particlesPerArm;
        const uint32_t indexInArm = index % particlesPerArm;
        const float progress = static_cast<float>(indexInArm) / static_cast<float>(particlesPerArm);
        const float armOffset = (armIndex % 2) * std::numbers::pi_v<float>;

        newParticle.isSpiral = true;
        newParticle.spiralCenter = translate;
        newParticle.spiralRight = cameraRight;
        newParticle.spiralUp = cameraUp;
        newParticle.spiralAngle = twoRotations * progress + armOffset;
        newParticle.spiralRadius = minimumRadius + (maximumRadius - minimumRadius) * progress;
        newParticle.spiralAngularVelocity = 1.8f;
        newParticle.spiralRadialVelocity = -0.08f * scaleMultiplier;

        const float spiralX = std::cos(newParticle.spiralAngle) * newParticle.spiralRadius;
        const float spiralY = std::sin(newParticle.spiralAngle) * newParticle.spiralRadius;
        newParticle.transform.translate = {
            translate.x + cameraRight.x * spiralX + cameraUp.x * spiralY,
            translate.y + cameraRight.y * spiralX + cameraUp.y * spiralY,
            translate.z + cameraRight.z * spiralX + cameraUp.z * spiralY
        };
        newParticle.transform.scale = { 0.08f * scaleMultiplier, 0.08f * scaleMultiplier, 0.08f * scaleMultiplier };
        newParticle.velocity = { 0.0f, 0.0f, 0.0f };
        newParticle.color = (armIndex % 2 == 0)
            ? Vector4{ 1.0f, 0.96f, 0.72f, 0.9f }
            : Vector4{ 1.0f, 0.62f, 0.08f, 0.9f };
        newParticle.lifeTime = 4.0f;
        newParticle.currentTime = 0.0f;
        newParticle.receivesWind = false;
        group.particles.push_back(newParticle);
    }
}

