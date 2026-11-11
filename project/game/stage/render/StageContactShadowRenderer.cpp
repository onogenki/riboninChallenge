#include "StageContactShadowRenderer.h"

#include "Camera.h"
#include "CarryableMirror.h"
#include "FixedMirror.h"
#include "Mirror.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Object3dFactory.h"
#include "Player.h"
#include "StageLighting.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

using namespace MyMath;

// 前方宣言した影用Objectを、この実装ファイルで生成します。
StageContactShadowRenderer::StageContactShadowRenderer() = default;

// Stage終了時に影用Objectだけを解放し、共通Modelは所有しません。
StageContactShadowRenderer::~StageContactShadowRenderer() = default;

// 既存のplaneと円画像を読み、専用Object以外の材質へ影響させません。
bool StageContactShadowRenderer::Initialize(Engine::Object3dCommon* object3dCommon)
{
	shadow_.reset();
	if (!object3dCommon) {
		return false;
	}
	auto shadow = std::make_unique<Engine::Object3d>();
	if (!Engine::Object3dFactory::InitializeObject(*shadow, object3dCommon, "debug/plane.obj") ||
		!shadow->SetTextureOverride("resources/debug/circle.png")) {
		return false;
	}
	shadow->SetContactShadowMask(true);
	shadow_ = std::move(shadow);
	return true;
}

// Playerの接地影と通常・危険Lightの方向影、鏡の床影をそれぞれ同じ床面へ描きます。
void StageContactShadowRenderer::Draw(
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
	const std::vector<OBB>& groundObbs)
{
	if (!shadow_) {
		return;
	}

	float groundY = 0.0f;
	if (player && playerShadowVisibility > 0.001f) {
		const Sphere& sphere = player->GetSphere();
		const float bottomY = sphere.center.y - sphere.radius;
		if (FindGroundY(sphere.center, bottomY, groundObbs, groundY)) {
			// 着地中は球の外まで見せ、ジャンプが高いほど影を小さく薄くします。
			const float height = (std::max)(bottomY - groundY, 0.0f);
			const float airRatio = std::clamp(height / 1.5f, 0.0f, 1.0f);
			const float size = 1.0f - 0.38f * airRatio;
			const float visibility = playerShadowVisibility * (1.0f - 0.55f * airRatio);
			const float warning = std::clamp(warningLightStrength / 0.72f, 0.0f, 1.0f);
			const float ordinary = std::clamp(ordinaryLightStrength, 0.0f, 1.0f);
			Vector3 floorPosition = sphere.center;
			const Vector3& move = player->GetMoveDirection();
			const float moveOffset = 0.22f * airRatio * (1.0f - warning);
			floorPosition.x += move.x * moveOffset;
			floorPosition.z += move.z * moveOffset;

			// 危険Lightを優先し、通常Lightも近づけば反対側へ短い影を伸ばします。
			const bool useWarning = warning > 0.08f;
			const Vector3& lightDirection = useWarning ? warningLightDirection : ordinaryLightDirection;
			const float lightStrength = useWarning ? warning : ordinary;
			const Vector3 lightOnFloor{ lightDirection.x, 0.0f, lightDirection.z };
			if (lightStrength > 0.02f && Length(lightOnFloor) > 0.08f) {
				const Vector3 towardLight = Normalize(lightOnFloor);
				float length = useWarning ? 0.30f + 1.15f * warning : 0.20f + 0.70f * ordinary;
				// 床の端では長さを半分ずつ試し、影が床からはみ出す場合だけ短くします。
				bool tailHasFloor = false;
				for (int attempt = 0; attempt < 3; ++attempt) {
					const Vector3 farEnd{
						sphere.center.x - towardLight.x * length * 1.20f,
						groundY,
						sphere.center.z - towardLight.z * length * 1.20f,
					};
					float farGroundY = 0.0f;
					if (FindGroundY(farEnd, bottomY, groundObbs, farGroundY) &&
						std::abs(farGroundY - groundY) < 0.05f) {
						tailHasFloor = true;
						break;
					}
					length *= 0.5f;
				}
				const Vector3 awayCenter{
					sphere.center.x - towardLight.x * length * 0.55f,
					groundY,
					sphere.center.z - towardLight.z * length * 0.55f,
				};
				if (tailHasFloor && length > 0.12f) {
					DrawPlane(camera, awayCenter, groundY,
						sphere.radius * 0.38f * size, length * 0.75f * size,
						std::atan2(towardLight.x, towardLight.z),
						(useWarning ? 0.24f : 0.26f) * lightStrength * visibility, 0.015f);
				}
			}

			// 三層を球の近くへ絞り、上方からのLightでも接点の濃さが近さに応じて変わるようにします。
			DrawPlane(camera, floorPosition, groundY,
				sphere.radius * 1.45f * size, sphere.radius * 1.15f * size,
				0.0f, 0.34f * visibility, 0.020f);
			// 球の足元の左右にも暗さを残し、離陸したら接触部分だけ先に消します。
			const float contact = 1.0f - airRatio;
			DrawPlane(camera, floorPosition, groundY,
				sphere.radius * 1.35f * size, sphere.radius * 1.05f * size,
				0.0f, (0.62f + 0.25f * ordinary + 0.25f * warning) *
					contact * contact * playerShadowVisibility, 0.022f);
			DrawPlane(camera, floorPosition, groundY,
				sphere.radius * 1.05f * size, sphere.radius * 0.82f * size,
				0.0f, (0.50f + 0.35f * ordinary + 0.24f * warning) * visibility, 0.025f);
		}
	}

	if (carryableMirror) {
		const OBB& obb = carryableMirror->GetObb();
		const float bottomY = GetBottomY(obb);
		// 置いた鏡はColliderの下端が床に少し埋まるため、中心より下の床を探します。
		if (FindGroundY(obb.center, obb.center.y, groundObbs, groundY)) {
			const StageLighting::ShadowInfluence light = lighting
				? lighting->GetShadowInfluence(obb.center) : StageLighting::ShadowInfluence{};
			DrawMirrorShadow(camera, obb.center, bottomY, groundY,
				std::clamp(carryableMirror->GetMirror().GetWidth() * 0.5f, 0.5f, 2.2f),
				0.70f, carryableMirror->GetObject().GetRotate().y, 0.46f,
				light.direction, light.strength, groundObbs);
		}
	}

	for (const std::unique_ptr<FixedMirror>& fixedMirror : fixedMirrors) {
		if (!fixedMirror) {
			continue;
		}
		const OBB& obb = fixedMirror->GetObb();
		const float bottomY = GetBottomY(obb);
		if (FindGroundY(obb.center, obb.center.y, groundObbs, groundY)) {
			const StageLighting::ShadowInfluence light = lighting
				? lighting->GetShadowInfluence(obb.center) : StageLighting::ShadowInfluence{};
			DrawMirrorShadow(camera, obb.center, bottomY, groundY,
				std::clamp(fixedMirror->GetMirror().GetWidth() * 0.5f, 0.5f, 2.2f),
				0.70f, fixedMirror->GetYaw(), 0.42f,
				light.direction, light.strength, groundObbs);
		}
	}
}

// 傾けた鏡でもColliderの最下点を使い、床との距離を過大評価しません。
float StageContactShadowRenderer::GetBottomY(const OBB& obb)
{
	const float verticalExtent =
		std::abs(obb.orientations[0].y) * obb.size.x +
		std::abs(obb.orientations[1].y) * obb.size.y +
		std::abs(obb.orientations[2].y) * obb.size.z;
	return obb.center.y - verticalExtent;
}

// 対象のX-Zが載るColliderだけを調べ、最も近い下側の床上面を返します。
bool StageContactShadowRenderer::FindGroundY(
	const Vector3& position,
	float maxGroundY,
	const std::vector<OBB>& groundObbs,
	float& groundY)
{
	bool found = false;
	for (const OBB& obb : groundObbs) {
		// 垂直な壁は床として扱わず、横向きの上面だけを候補にします。
		if (obb.orientations[1].y < 0.8f) {
			continue;
		}
		const Vector3 offset{ position.x - obb.center.x, 0.0f, position.z - obb.center.z };
		if (std::abs(Dot(offset, obb.orientations[0])) > obb.size.x ||
			std::abs(Dot(offset, obb.orientations[2])) > obb.size.z) {
			continue;
		}
		const float topY = obb.center.y + obb.size.y * obb.orientations[1].y;
		if (topY > maxGroundY + 0.10f || (found && topY <= groundY)) {
			continue;
		}
		groundY = topY;
		found = true;
	}
	return found;
}

// 鏡の幅に沿う三層を重ね、Playerと同じ接地・浮上・通常Lightの変化を床へ示します。
void StageContactShadowRenderer::DrawMirrorShadow(
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
	const std::vector<OBB>& groundObbs)
{
	const float height = (std::max)(bottomY - groundY, 0.0f);
	const float airRatio = std::clamp(height / 1.5f, 0.0f, 1.0f);
	const float size = 1.0f - 0.38f * airRatio;
	const float visibility = 1.0f - 0.55f * airRatio;
	const float contact = 1.0f - airRatio;
	const float ordinary = std::clamp(lightStrength, 0.0f, 1.0f);
	// 鏡が床へ接している時だけ中心を濃くし、持ち上げた時は三層を縮めます。
	DrawPlane(camera, position, groundY,
		radiusX * 0.95f * size, radiusZ * 1.15f * size, yaw,
		baseOpacity * 0.65f * visibility, 0.020f);
	DrawPlane(camera, position, groundY,
		radiusX * 0.85f * size, radiusZ * size, yaw,
		baseOpacity * (0.85f + 0.25f * ordinary) * contact * contact, 0.022f);
	DrawPlane(camera, position, groundY,
		radiusX * 0.70f * size, radiusZ * 0.82f * size, yaw,
		baseOpacity * (0.80f + 0.35f * ordinary) * visibility, 0.025f);

	// Lightが横から届く時だけ反対側へ短く伸ばし、床の外へは描きません。
	const Vector3 lightOnFloor{ lightDirection.x, 0.0f, lightDirection.z };
	if (ordinary <= 0.02f || Length(lightOnFloor) <= 0.08f) {
		return;
	}
	const Vector3 towardLight = Normalize(lightOnFloor);
	const float length = 0.20f + 0.70f * ordinary;
	const Vector3 farEnd{
		position.x - towardLight.x * length * 1.20f,
		groundY,
		position.z - towardLight.z * length * 1.20f,
	};
	float farGroundY = 0.0f;
	if (!FindGroundY(farEnd, position.y, groundObbs, farGroundY) ||
		std::abs(farGroundY - groundY) >= 0.05f) {
		return;
	}
	const Vector3 awayCenter{
		position.x - towardLight.x * length * 0.55f,
		groundY,
		position.z - towardLight.z * length * 0.55f,
	};
	DrawPlane(camera, awayCenter, groundY,
		radiusZ * 0.70f * size, length * 0.75f * size,
		std::atan2(towardLight.x, towardLight.z),
		baseOpacity * 0.45f * ordinary * visibility, 0.015f);
}

// 描画ごとの定数をObject3dへ渡し、同じplaneを使う各層の位置と濃さを固定します。
void StageContactShadowRenderer::DrawPlane(
	Engine::Camera& camera,
	const Vector3& position,
	float groundY,
	float radiusX,
	float radiusZ,
	float yaw,
	float opacity,
	float surfaceOffset)
{
	shadow_->SetTranslate({ position.x, groundY + surfaceOffset, position.z });
	shadow_->SetRotate({ -std::numbers::pi_v<float> * 0.5f, yaw, 0.0f });
	shadow_->SetScale({ radiusX, radiusZ, 1.0f });
	shadow_->SetOpacity(opacity);
	shadow_->SetCamera(&camera);
	shadow_->Update();
	shadow_->Draw();
}
