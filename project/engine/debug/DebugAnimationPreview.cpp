#include "DebugAnimationPreview.h"

#include "Camera.h"
#include "GPUParticle.h"
#include "ImGuiManager.h"
#include "Input.h"
#include "MyMath.h"
#include "Object3dFactory.h"
#include "Object3dRenderContext.h"
#include <cmath>
#include <dinput.h>

using namespace MyMath;

// 人型・歩行モデル・手持ちWeaponを作り、Animation確認を始める準備をします。
bool DebugAnimationPreview::Initialize(
	Object3dCommon* object3dCommon,
	std::vector<std::unique_ptr<Object3d>>& animationObjects)
{
	if (!object3dCommon) {
		return false;
	}

	walkAnimation_ = Model::LoadAnimationFile("./resources", "debug/walk.gltf");
	humanAnimation_ = Model::LoadAnimationFile("./resources", "debug/human.gltf");

	auto humanObject = Object3dFactory::Create(object3dCommon, "debug/human.gltf", true);
	auto handWeapon = Object3dFactory::Create(object3dCommon, "debug/sphere.obj");
	auto walkObject = Object3dFactory::Create(object3dCommon, "debug/walk.gltf", true);
	if (!humanObject || !handWeapon || !walkObject) {
		return false;
	}

	humanObject->SetEnvironmentCoefficient(0.3f);
	humanObject->GetTransform().translate = { 2.0f, 0.0f, 0.0f };
	humanObject->GetTransform().rotate = { 0.0f, 0.0f, 0.0f };
	humanObject->GetTransform().scale = { 0.2f, 0.2f, 0.2f };
	humanObject->PlayAnimation(humanAnimation_);
	humanObject->SetIsLoop(false);

	handWeapon->GetModel()->SetTexture("Resources/debug/monsterBall.png");
	handWeapon->GetTransform().scale = { 1.5f, 1.5f, 1.5f };
	handWeapon->GetTransform().translate = { 0.0f, 0.25f, 0.0f };

	walkObject->SetEnvironmentCoefficient(0.3f);
	walkObject->GetTransform().translate = { -2.0f, 0.0f, 0.0f };
	walkObject->GetTransform().scale = { 1.0f, 1.0f, 1.0f };
	walkObject->PlayAnimation(walkAnimation_);
	walkObject->SetIsLoop(true);

	attachmentSource_ = humanObject.get();
	walkObject_ = walkObject.get();
	handWeapon_ = std::move(handWeapon);
	animationObjects.push_back(std::move(humanObject));
	animationObjects.push_back(std::move(walkObject));
	return true;
}

// 歩行入力、Animation行列、手持ちモデル、足跡Particleを順番に更新します。
void DebugAnimationPreview::Update(const Context& context)
{
	Vector3 moveDirection{};
	Input* input = Input::GetInstance();
	if (context.acceptsGameInput && input->PushKey(DIK_W)) {
		moveDirection.z += 1.0f;
	}
	if (context.acceptsGameInput && input->PushKey(DIK_S)) {
		moveDirection.z -= 1.0f;
	}
	if (context.acceptsGameInput && input->PushKey(DIK_A)) {
		moveDirection.x -= 1.0f;
	}
	if (context.acceptsGameInput && input->PushKey(DIK_D)) {
		moveDirection.x += 1.0f;
	}

	if (walkObject_) {
		if (Length(moveDirection) > 0.0f) {
			moveDirection = Normalize(moveDirection);
			walkObject_->GetTransform().translate.x += moveDirection.x * 2.0f * context.deltaTime;
			walkObject_->GetTransform().translate.z += moveDirection.z * 2.0f * context.deltaTime;
			// 移動ベクトルからY回転を作り、モデル正面を歩行方向へ合わせます。
			walkObject_->GetTransform().rotate.y = std::atan2(moveDirection.x, moveDirection.z);
			walkObject_->SetAnimationPlaying(true);
		} else {
			walkObject_->SetAnimationPlaying(false);
		}
	}

	// Animationモデルも通常モデルと同じ描画準備を行います。
	if (context.animationObjects && context.renderContext) {
		context.renderContext->UpdateObjects(*context.animationObjects);
	}

	Matrix4x4 handWorldMatrix{};
	if (attachmentSource_ && handWeapon_ &&
		attachmentSource_->GetJointWorldMatrix("ボーン.016", handWorldMatrix)) {
		handWeapon_->SetParentWorldMatrix(handWorldMatrix);
		if (context.renderContext) {
			context.renderContext->UpdateObject(*handWeapon_);
		}
	}

	Matrix4x4 leftFootWorldMatrix{};
	Matrix4x4 rightFootWorldMatrix{};
	const bool hasLeftFoot = walkObject_ &&
		walkObject_->GetJointWorldMatrix("mixamorig:LeftFoot", leftFootWorldMatrix);
	const bool hasRightFoot = walkObject_ &&
		walkObject_->GetJointWorldMatrix("mixamorig:RightFoot", rightFootWorldMatrix);
	GPUParticle* gpuParticle = GPUParticle::GetInstance();
	if (Length(moveDirection) > 0.0f && (hasLeftFoot || hasRightFoot)) {
		// 足ボーンのWorld行列の移動成分を取り出し、足元Particleの発生位置へ使います。
		gpuParticle->SetEmitterTranslate(0, {
			leftFootWorldMatrix.m[3][0], leftFootWorldMatrix.m[3][1], leftFootWorldMatrix.m[3][2] });
		gpuParticle->SetEmitterEnabled(0, hasLeftFoot);
		gpuParticle->SetEmitterTranslate(1, {
			rightFootWorldMatrix.m[3][0], rightFootWorldMatrix.m[3][1], rightFootWorldMatrix.m[3][2] });
		gpuParticle->SetEmitterEnabled(1, hasRightFoot);
	} else {
		gpuParticle->SetEmitterEnabled(0, false);
		gpuParticle->SetEmitterEnabled(1, false);
	}

	if (context.directionalLight) {
		gpuParticle->SetDirectionalLight(
			context.directionalLight->color,
			context.directionalLight->direction,
			context.directionalLight->intensity);
	}
	gpuParticle->Update(context.deltaTime);
}

// 人型モデル付近へParticleを出すための基準座標を返します。
Vector3 DebugAnimationPreview::GetParticleEffectPosition() const
{
	if (!attachmentSource_) {
		return {};
	}

	Vector3 effectPosition = attachmentSource_->GetTransform().translate;
	effectPosition.x += 1.0f;
	effectPosition.y += 1.0f;
	return effectPosition;
}

// 人型AnimationのWorld行列を作り、Game ViewのCameraへ合わせてSkeletonを重ねます。
void DebugAnimationPreview::DrawSkeletonDebug(const Camera* camera, bool isEnabled) const
{
	if (!isEnabled || !camera || !attachmentSource_ || !attachmentSource_->IsSkeletal()) {
		return;
	}

	const Transform& animationTransform = attachmentSource_->GetTransform();
	const Matrix4x4 animationWorldMatrix = MakeAffineMatrix(
		animationTransform.scale,
		animationTransform.rotate,
		animationTransform.translate);
	ImGuiManager::GetInstance()->SkeletonDebugDraw(
		attachmentSource_->GetSkeleton(),
		animationWorldMatrix,
		camera->GetViewProjectionMatrix());
}

// Scene終了時に、非所有ポインタとWeaponをまとめて解放します。
void DebugAnimationPreview::Finalize()
{
	attachmentSource_ = nullptr;
	walkObject_ = nullptr;
	handWeapon_.reset();
	walkAnimation_ = {};
	humanAnimation_ = {};
}
