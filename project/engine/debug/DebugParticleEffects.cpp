#include "DebugParticleEffects.h"

#include "GPUParticle.h"
#include "Input.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include <algorithm>
#include <dinput.h>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

// Debug画面のEmitterとEffectが共通で使うTexture・Particle Groupを一度だけ準備します。
void DebugParticleEffects::InitializeResources()
{
	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->LoadTexture("Resources/debug/uvChecker.png");
	textureManager->LoadTexture("Resources/debug/circle.png");
	textureManager->LoadTexture("Resources/debug/circle2.png");
	textureManager->LoadTexture("Resources/debug/gradationLine.png");

	ParticleManager* particleManager = ParticleManager::GetInstance();
	particleManager->CreateParticleGroup("Circle", "Resources/debug/circle.png");
	particleManager->CreateParticleGroup("Plane", "Resources/debug/uvChecker.png");
	particleManager->CreateParticleGroup("Hit", "Resources/debug/circle2.png");
	particleManager->CreateRingParticleGroup("Ring", "Resources/debug/gradationLine.png");
	particleManager->CreateCylinderParticleGroup("Cylinder", "Resources/debug/gradationLine.png");
	particleManager->CreateParticleGroup("PillarSparkle", "Resources/debug/circle2.png");
	particleManager->CreateParticleGroup("LightCore", "Resources/debug/circle2.png");
	particleManager->CreateParticleGroup("LightRain", "Resources/debug/gradationLine.png");
	particleManager->CreateParticleGroup("LightSpiral", "Resources/debug/circle2.png");

	GPUParticle* gpuParticle = GPUParticle::GetInstance();
	gpuParticle->SetEmitterType(0, GPUParticle::EmitterType::Mix);
	gpuParticle->SetEmitterParticleType(0, GPUParticle::ParticleType::Trail);
	gpuParticle->SetEmitterType(1, GPUParticle::EmitterType::Mix);
	gpuParticle->SetEmitterParticleType(1, GPUParticle::ParticleType::Trail);
}

// Particle種類ごとの有効状態・発生数・大きさをImGuiで編集します。
void DebugParticleEffects::DrawImGui(const Vector3& effectPosition, bool embedded)
{
#ifdef USE_IMGUI
	ParticleManager* particleManager = ParticleManager::GetInstance();
	if (!embedded) {
		ImGui::Begin("Particle Effects");
	}
	ImGui::Text("Emit position: %.2f, %.2f, %.2f", effectPosition.x, effectPosition.y, effectPosition.z);

	auto drawControl = [particleManager](const char* label, const char* groupName, EffectControl& control) {
		if (ImGui::TreeNode(label)) {
			ImGui::Checkbox("Enable", &control.enabled);
			ImGui::SliderInt("Emit Count", &control.emitCount, 1, 100);
			ImGui::SliderFloat("Scale", &control.scale, 0.1f, 5.0f, "%.2f");
			if (ImGui::Checkbox("Billboard", &control.billboard)) {
				particleManager->SetBillboardEnabled(groupName, control.billboard);
			}
			ImGui::TreePop();
		}
	};

	drawControl("Hit Slash", "Hit", hitEffect_);
	drawControl("Impact Ring", "Ring", ringEffect_);
	if (ImGui::TreeNode("Portal Cylinder")) {
		if (ImGui::Checkbox("Display", &cylinderEffect_.enabled)) {
			refreshCylinder_ = true;
		}
		ImGui::SliderInt("Emit Count", &cylinderEffect_.emitCount, 1, 100);
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			refreshCylinder_ = true;
		}
		ImGui::SliderFloat("Scale", &cylinderEffect_.scale, 0.1f, 5.0f, "%.2f");
		if (ImGui::IsItemDeactivatedAfterEdit()) {
			refreshCylinder_ = true;
		}
		if (ImGui::Checkbox("Billboard", &cylinderEffect_.billboard)) {
			particleManager->SetBillboardEnabled("Cylinder", cylinderEffect_.billboard);
			refreshCylinder_ = true;
		}
		ImGui::TreePop();
	}
	drawControl("Pillar Sparkle", "PillarSparkle", pillarSparkleEffect_);
	drawControl("Light Core", "LightCore", lightCoreEffect_);
	drawControl("Light Rain", "LightRain", lightRainEffect_);
	drawControl("Light Spiral", "LightSpiral", lightSpiralEffect_);

	if (ImGui::Button("Clear All Effects")) {
		Clear();
	}
	if (!embedded) {
		ImGui::End();
	}
#else
	(void)effectPosition;
	(void)embedded;
#endif
}

// 一フレームのParticle発生とShortcut入力を同じ部品内で完結させます。
void DebugParticleEffects::UpdateFrame(const Vector3& effectPosition, float deltaTime)
{
	Update(effectPosition, deltaTime);
	HandleShortcutInput(effectPosition);
}

// 有効なParticleを指定位置へ発生させ、Cylinder設定変更時だけ作り直します。
void DebugParticleEffects::Update(const Vector3& effectPosition, float deltaTime)
{
	ParticleManager* particleManager = ParticleManager::GetInstance();
	if (cylinderEffect_.enabled != isCylinderVisible_ || refreshCylinder_) {
		particleManager->ClearParticles("Cylinder");
		if (cylinderEffect_.enabled) {
			particleManager->SetBillboardEnabled("Cylinder", cylinderEffect_.billboard);
			particleManager->EmitCylinderEffect(
				"Cylinder",
				static_cast<uint32_t>(cylinderEffect_.emitCount),
				effectPosition,
				cylinderEffect_.scale);
		}
		isCylinderVisible_ = cylinderEffect_.enabled;
		refreshCylinder_ = false;
	}

	// Hitなどは0.12秒ごとに発生させ、1フレームに大量発生しないようにします。
	emitTimer_ += (std::max)(deltaTime, 0.0f);
	if (emitTimer_ < 0.12f) {
		return;
	}
	emitTimer_ = 0.0f;

	if (hitEffect_.enabled) {
		particleManager->EmitHitEffect("Hit", static_cast<uint32_t>(hitEffect_.emitCount), effectPosition, hitEffect_.scale);
	}
	if (ringEffect_.enabled) {
		particleManager->EmitRingEffect("Ring", static_cast<uint32_t>(ringEffect_.emitCount), effectPosition, ringEffect_.scale);
	}
	if (pillarSparkleEffect_.enabled) {
		particleManager->EmitPillarSparkle("PillarSparkle", static_cast<uint32_t>(pillarSparkleEffect_.emitCount), effectPosition, pillarSparkleEffect_.scale);
	}
	if (lightCoreEffect_.enabled) {
		particleManager->EmitLightCore("LightCore", static_cast<uint32_t>(lightCoreEffect_.emitCount), effectPosition, lightCoreEffect_.scale);
	}
	if (lightRainEffect_.enabled) {
		particleManager->EmitLightRain("LightRain", static_cast<uint32_t>(lightRainEffect_.emitCount), effectPosition, lightRainEffect_.scale);
	}
	if (lightSpiralEffect_.enabled) {
		particleManager->EmitLightSpiral("LightSpiral", static_cast<uint32_t>(lightSpiralEffect_.emitCount), effectPosition, lightSpiralEffect_.scale);
	}
}

// キー操作用にCylinderの表示状態を反転し、その場で見た目を更新します。
void DebugParticleEffects::ToggleCylinder(const Vector3& effectPosition)
{
	cylinderEffect_.enabled = !cylinderEffect_.enabled;
	refreshCylinder_ = true;
	Update(effectPosition, 0.0f);
}

// キー操作用にHitとRingを一度だけ同じ位置へ発生させます。
void DebugParticleEffects::EmitHitAndRing(const Vector3& effectPosition)
{
	ParticleManager* particleManager = ParticleManager::GetInstance();
	particleManager->EmitHitEffect("Hit", static_cast<uint32_t>(hitEffect_.emitCount), effectPosition, hitEffect_.scale);
	particleManager->EmitRingEffect("Ring", static_cast<uint32_t>(ringEffect_.emitCount), effectPosition, ringEffect_.scale);
}

// Debug用の0・Pキー入力を読み、Particle表示・単発発生を切り替えます。
void DebugParticleEffects::HandleShortcutInput(const Vector3& effectPosition)
{
	Input* input = Input::GetInstance();
	if (input->TriggerKey(DIK_0)) {
		ToggleCylinder(effectPosition);
	}
	if (input->TriggerKey(DIK_P)) {
		EmitHitAndRing(effectPosition);
	}
}

// 全種類のParticleを消し、ImGui上の有効状態も初期状態へ戻します。
void DebugParticleEffects::Clear()
{
	ParticleManager* particleManager = ParticleManager::GetInstance();
	for (const char* groupName : {
		"Hit", "Ring", "Cylinder", "PillarSparkle", "LightCore", "LightRain", "LightSpiral" }) {
		particleManager->ClearParticles(groupName);
	}
	isCylinderVisible_ = false;
	hitEffect_.enabled = false;
	ringEffect_.enabled = false;
	cylinderEffect_.enabled = false;
	pillarSparkleEffect_.enabled = false;
	lightCoreEffect_.enabled = false;
	lightRainEffect_.enabled = false;
	lightSpiralEffect_.enabled = false;
	emitTimer_ = 0.0f;
	refreshCylinder_ = false;
}
