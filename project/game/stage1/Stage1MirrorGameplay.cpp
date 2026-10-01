#include "Stage1.h"

#include "CarryableMirror.h"
#include "DirectXCommon.h"
#include "FixedMirror.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "Player.h"
#include <dinput.h>

using namespace MyMath;

// Player自身の向きを携帯鏡へ渡し、操作・Puzzle Laserの反射・Player接触を更新します。
void Stage1::UpdateMirrorGameplay()
{
	// Eキー・Mouse入力をCarryableMirrorへ渡し、鏡のTransformとLaser反射経路を同じフレームで更新します。
	if (!player_ || !carryableMirror_) {
		return;
	}

	Input* input = Input::GetInstance();
	const Vector2 mouseScreen = input->GetMouseScreen();
	// ImGuiのInspectorやSliderを右クリックしても、Game View外ならMirror操作へ渡しません。
	const bool isMouseOverGameView =
		ImGuiManager::GetInstance()->IsGameViewActive() &&
		ImGuiManager::GetInstance()->IsMouseOverGameView(mouseScreen.x, mouseScreen.y);
	const bool interactPressed =
		ImGuiManager::GetInstance()->IsGameViewActive() &&
		input->TriggerKey(DIK_E);
	// 左クリックは縦向きの盾、右クリックの短押しは水平Mirrorの表裏切替に使用します。
	const bool isMirrorAiming =
		isMouseOverGameView &&
		carryableMirror_->IsCarried() &&
		input->IsMouseButtonPressed(0);
	const bool isHorizontalMirrorHeld =
		isMouseOverGameView &&
		carryableMirror_->IsCarried() &&
		input->IsMouseButtonPressed(1);
	const float mirrorAimMouseX = (isMirrorAiming || isHorizontalMirrorHeld)
		? static_cast<float>(input->GetMouseX())
		: 0.0f;
	// 右クリック中はMouse横移動で左右へ向け、Mouse縦移動で水平Mirrorを前後へ傾けます。
	const float mirrorAimMouseY = isHorizontalMirrorHeld
		? static_cast<float>(input->GetMouseY())
		: 0.0f;
	// Cameraが別の方向を向いていても、鏡はPlayerが向く正面へ追従させます。
	carryableMirror_->Update(
		DirectXCommon::GetInstance()->GetDeltaTime(),
		player_->GetPosition(),
		player_->GetFacingYaw(),
		interactPressed,
		isMirrorAiming,
		mirrorAimMouseX,
		isHorizontalMirrorHeld,
		mirrorAimMouseY);
}

// 固定鏡・鏡床・携帯鏡を、Laser経路計算で使うMirror一覧へまとめます。
std::vector<const Mirror*> Stage1::GetLaserReflectors() const
{
	std::vector<const Mirror*> mirrors;
	mirrors.reserve(fixedMirrors_.size() + 2);
	for (const auto& fixedMirror : fixedMirrors_) {
		if (fixedMirror) {
			mirrors.push_back(&fixedMirror->GetMirror());
		}
	}
	if (mirrorFloor_) {
		mirrors.push_back(&mirrorFloor_->GetMirror());
	}
	if (carryableMirror_) {
		mirrors.push_back(&carryableMirror_->GetMirror());
	}
	return mirrors;
}

// 危険Lightの各線分をLaserとして再計算し、Mirror反射後の経路を返します。
std::vector<LaserSegment> Stage1::ReflectHazardLightSegments(
	const std::vector<LaserSegment>& sourceSegments) const
{
	std::vector<LaserSegment> reflectedSegments;
	const std::vector<const Mirror*> mirrors = GetLaserReflectors();
	for (const LaserSegment& sourceSegment : sourceSegments) {
		const Vector3 direction{
			sourceSegment.end.x - sourceSegment.start.x,
			sourceSegment.end.y - sourceSegment.start.y,
			sourceSegment.end.z - sourceSegment.start.z,
		};
		const float distance = Length(direction);
		if (distance <= 0.0001f) {
			continue;
		}

		// 危険Lightも通常Laserと同じ経路計算を使い、最大3回まで反射させます。
		Laser reflectedLight;
		reflectedLight.SetOrigin(sourceSegment.start);
		reflectedLight.SetDirection(direction);
		reflectedLight.SetMaxDistance(distance);
		reflectedLight.SetMaxReflectionCount(3);
		// 危険Lightも、反射先を求める前に床・壁・Doorで遮られます。
		reflectedLight.Update(mirrors, collisionWorld_.GetLightBlockingObbs(), 0.06f);
		const std::vector<LaserSegment>& segments = reflectedLight.GetSegments();
		reflectedSegments.insert(reflectedSegments.end(), segments.begin(), segments.end());
	}
	return reflectedSegments;
}
