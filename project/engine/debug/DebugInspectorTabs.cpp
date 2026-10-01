#include "DebugInspectorTabs.h"

#include "CameraManager.h"
#include "DebugEcsInspector.h"
#include "DebugParticleEffects.h"
#include "ImGuiManager.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"

// 一つのEmitterに共通する数・頻度・大きさ・Windの編集UIを描画します。
void DebugInspectorTabs::DrawEmitterControl(const char* label, ParticleEmitter* emitter)
{
#ifdef USE_IMGUI
	if (!emitter || !ImGui::TreeNode(label)) {
		return;
	}

	int count = static_cast<int>(emitter->GetCount());
	if (ImGui::SliderInt("Emit Count", &count, 1, 100)) {
		emitter->SetCount(static_cast<uint32_t>(count));
	}

	float frequency = emitter->GetFrequency();
	if (ImGui::SliderFloat("Frequency", &frequency, 0.02f, 2.0f, "%.2f")) {
		emitter->SetFrequency(frequency);
	}

	Vector3 scale = emitter->GetTransform().scale;
	if (ImGui::SliderFloat("Scale", &scale.x, 0.1f, 5.0f, "%.2f")) {
		scale.y = scale.x;
		scale.z = scale.x;
		emitter->SetScale(scale);
	}

	bool receivesWind = emitter->GetReceivesWind();
	if (ImGui::Checkbox("Receives Wind", &receivesWind)) {
		emitter->SetReceivesWind(receivesWind);
	}
	ImGui::TreePop();
#else
	static_cast<void>(label);
	static_cast<void>(emitter);
#endif
}

// Debug専用のInspectorタブをまとめて描画します。
void DebugInspectorTabs::Draw(const Context& context)
{
#ifdef USE_IMGUI
	if (context.emitterTransform && context.activeEmitter &&
		ImGui::BeginTabItem("Particle")) {
		const std::string particleRequest =
			ImGuiManager::GetInstance()->ParticleWindow(*context.emitterTransform, true);
		if (particleRequest == "Circle") {
			*context.activeEmitter = context.emitterCircle;
		} else if (particleRequest == "Plane") {
			*context.activeEmitter = context.emitterPlane;
		}
		if (*context.activeEmitter) {
			(*context.activeEmitter)->SetTranslate(context.emitterTransform->translate);
		}

		DrawEmitterControl("Circle Particle", context.emitterCircle);
		DrawEmitterControl("Plane Particle", context.emitterPlane);

		if (ImGui::Button("Rain Wind")) {
			ParticleManager::GetInstance()->SetWindEnabled(true);
			ParticleManager::GetInstance()->SetWindAcceleration({ 0.0f, -0.35f, 0.0f });
			if (context.emitterCircle) {
				context.emitterCircle->SetReceivesWind(true);
			}
		}
		ImGui::EndTabItem();
	}
	if (context.particleEffects && ImGui::BeginTabItem("Effect")) {
		context.particleEffects->DrawImGui(context.effectPosition, true);
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("Camera")) {
		ImGuiManager::GetInstance()->CameraWindow(context.cameraManager, true);
		ImGui::EndTabItem();
	}
	DebugEcsInspector::Draw({ context.ecsWorld, context.selectedEcsEntity });
#else
	static_cast<void>(context);
#endif
}
