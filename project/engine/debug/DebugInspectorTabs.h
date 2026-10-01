#pragma once

#include "../ecs/EcsWorld.h"
#include "Transform.h"
#include "Vector3.h"

class CameraManager;
class DebugParticleEffects;
class ParticleEmitter;

// DebugSceneのInspectorへ追加する、Particle・Effect・Camera・ECSのDebugタブです。
// 本編のObjectやSpriteを所有せず、UIで編集する値だけをContext経由で受け取ります。
class DebugInspectorTabs
{
public:
	struct Context
	{
		// Particle設定WindowとEmitter位置が共有するTransformです。所有しません。
		Transform* emitterTransform = nullptr;
		// Circle・PlaneのEmitterです。所有しません。
		ParticleEmitter* emitterCircle = nullptr;
		ParticleEmitter* emitterPlane = nullptr;
		// Inspectorで選択中のEmitterを保存する場所です。所有しません。
		ParticleEmitter** activeEmitter = nullptr;
		// Effectタブが編集するParticle種類ごとの設定です。所有しません。
		DebugParticleEffects* particleEffects = nullptr;
		// Effectを発生させる3D座標です。
		Vector3 effectPosition{};
		// Cameraタブが編集するCamera一覧です。所有しません。
		CameraManager* cameraManager = nullptr;
		// ECSタブが表示・編集するWorldと選択中Entityです。所有しません。
		Ecs::World* ecsWorld = nullptr;
		Ecs::Entity* selectedEcsEntity = nullptr;
	};

	// Debug専用のInspectorタブをまとめて描画します。
	static void Draw(const Context& context);

private:
	// 一つのEmitterに共通する数・頻度・大きさ・Windの編集UIを描画します。
	static void DrawEmitterControl(const char* label, ParticleEmitter* emitter);
};
