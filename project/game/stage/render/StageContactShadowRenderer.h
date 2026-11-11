#pragma once

#include "MyMath.h"
#include <memory>
#include <vector>

class CarryableMirror;
class FixedMirror;
class Player;
class StageLighting;
namespace Engine { class Camera; }
namespace Engine { class Object3d; }
namespace Engine { class Object3dCommon; }

// 既存の白黒円画像を一枚の半透明planeで使い回し、Playerと鏡の高さを床影で示します。
class StageContactShadowRenderer
{
public:
	StageContactShadowRenderer();
	~StageContactShadowRenderer();

	// 影用planeと白黒画像の準備が両方できた時だけStageへ成功を返します。
	bool Initialize(Engine::Object3dCommon* object3dCommon);
	// 現在位置の下に床があるPlayerと鏡だけを描き、接地・浮上とLightの向きを床へ示します。
	void Draw(
		Engine::Camera& camera,
		const Player* player,
		float playerShadowVisibility,
		const Vector3& warningLightDirection,
		float warningLightStrength,
		const Vector3& ordinaryLightDirection,
		float ordinaryLightStrength,
		const StageLighting* lighting,
		const CarryableMirror* carryableMirror,
		const std::vector<std::unique_ptr<FixedMirror>>& fixedMirrors,
		const std::vector<MyMath::OBB>& groundObbs);

private:
	// Colliderの向きも含めて、鏡の最下点を求めます。
	static float GetBottomY(const MyMath::OBB& obb);
	// 対象の真下にある最も高い床面を選び、壁際や段差で空中に影を置きません。
	static bool FindGroundY(
		const Vector3& position,
		float maxGroundY,
		const std::vector<MyMath::OBB>& groundObbs,
		float& groundY);
	// 鏡の幅に合わせた接地影を重ね、高さと通常LightでPlayerと同じように変化させます。
	void DrawMirrorShadow(
		Engine::Camera& camera,
		const Vector3& position,
		float bottomY,
		float groundY,
		float radiusX,
		float radiusZ,
		float yaw,
		float baseOpacity,
		const Vector3& lightDirection,
		float lightStrength,
		const std::vector<MyMath::OBB>& groundObbs);
	// 一枚のplaneを描画ごとに配置し直し、各層の大きさと透明度を固定して渡します。
	void DrawPlane(
		Engine::Camera& camera,
		const Vector3& position,
		float groundY,
		float radiusX,
		float radiusZ,
		float yaw,
		float opacity,
		float surfaceOffset);

	std::unique_ptr<Engine::Object3d> shadow_;
};
