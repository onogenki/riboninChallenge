#include "DebugTimePlaybackSmoke.h"

#include "Object3d.h"
#include "ParticleManager.h"
#include <Windows.h>
#include <fstream>

using namespace MyMath;

// DebugSceneから渡された対象と操作窓口を使い、巻き戻し自動確認の初期状態を作ります。
void DebugTimePlaybackSmoke::Start(
	State& state,
	bool isUiSmokeEnabled,
	const std::string& modelFile,
	const std::string& timestamp,
	const Context& context)
{
	state.timePlaybackSmokeEnabled_ = true;
	state.timePlaybackSmokeFinished_ = false;
	state.timePlaybackSmokeStage_ = 0;
	state.timePlaybackSmokeStableFrames_ = 0;
	state.timePlaybackSmokeDeleteIterations_ = 0;
	state.timePlaybackSmokeStageTime_ = 0.0f;
	state.timePlaybackSmokeModelFile_.clear();

	std::error_code errorCode;
	std::filesystem::create_directories("logs", errorCode);
	state.timePlaybackSmokeLogPath_ =
		std::filesystem::path("logs") / ("time_playback_smoke_" + timestamp + ".log");

	if (!context.objectPlane || !context.objectAxis ||
		!context.clearAddedSceneModels || !context.resetDebugEffects) {
		Finish(state, false, "Required Debug test context was not initialized.");
		return;
	}
	if (isUiSmokeEnabled) {
		Finish(state, false, "CG2_DEBUG_UI_SMOKE cannot run at the same time.");
		return;
	}
	if (!context.objectPlane || !context.objectAxis || !context.objectAxis->IsAnimating()) {
		Finish(state, false, "Required normal and animation test objects were not initialized.");
		return;
	}

	if (modelFile.empty()) {
		Finish(state, false, "No loadable normal model was available for delete/reset stress.");
		return;
	}
	state.timePlaybackSmokeModelFile_ = modelFile;

	// Object3dとフレーム更新を使う前に、共通Controllerの基本動作を一定条件で確認します。
	Transform localOrigin{ { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
	Transform localMiddle{ { 1.2f, 1.1f, 1.4f }, { 0.1f, 0.2f, -0.1f }, { 1.0f, 2.0f, 0.0f } };
	Transform localTarget{ { 1.5f, 0.8f, 2.0f }, { 0.2f, -0.4f, 0.6f }, { 4.0f, -2.0f, 1.0f } };
	Transform localTransform = localMiddle;
	TransformPlaybackController controller;
	controller.RecordEdit(localOrigin, localTransform, 1.0f);
	controller.RecordEdit(localMiddle, localTarget, 1.0f);
	localTransform = localTarget;
	controller.SetReturning(true);
	controller.Update(localTransform, 0.5f);
	const Transform localPaused = localTransform;
	if (!controller.IsReturning() || !IsNearlyEqual(controller.GetPlaybackTime(), 1.5f) ||
		!IsNearlyEqual(controller.GetDuration(), 2.0f) ||
		!IsNearlyEqual(localPaused.translate, Vector3{ 2.5f, 0.0f, 0.5f })) {
		Finish(state, false, "A two-second transform edit did not reverse by exactly half a second.");
		return;
	}
	controller.SetReturning(false);
	controller.Update(localTransform, 0.25f);
	if (!IsNearlyEqual(localTransform, localPaused) || !controller.CanMoveForward()) {
		Finish(state, false, "Transform Return did not pause at the unchecked position.");
		return;
	}
	if (!controller.StartMoveForward()) {
		Finish(state, false, "Transform Move could not start after a paused Return.");
		return;
	}
	controller.Update(localTransform, 0.5f);
	if (!IsNearlyEqual(localTransform, localTarget) || controller.IsMovingForward()) {
		Finish(state, false, "Transform Move did not replay to the edited target.");
		return;
	}
	controller.SetReturning(true);
	controller.Update(localTransform, 2.0f);
	if (!IsNearlyEqual(localTransform, localOrigin) || controller.IsReturning() || !controller.CanMoveForward()) {
		Finish(state, false, "Transform Return did not auto-stop at its original position.");
		return;
	}

	TransformPlaybackController compactedController;
	Transform compactedTransform = localOrigin;
	for (int index = 0; index < 4096; ++index) {
		const Transform beforeEdit = compactedTransform;
		compactedTransform.translate.x += 0.001f;
		compactedController.RecordEdit(beforeEdit, compactedTransform, 0.01f);
	}
	const Transform compactedTarget = compactedTransform;
	if (!IsNearlyEqual(compactedController.GetDuration(), 40.96f, 0.01f)) {
		Finish(state, false, "Compacted transform history did not preserve its total edit time.");
		return;
	}
	compactedController.SetReturning(true);
	compactedController.Update(compactedTransform, 40.96f);
	if (!IsNearlyEqual(compactedTransform, localOrigin) || !compactedController.CanMoveForward()) {
		Finish(state, false, "Compacted transform history did not preserve its original endpoint.");
		return;
	}
	if (!compactedController.StartMoveForward()) {
		Finish(state, false, "Compacted transform history could not start Move.");
		return;
	}
	compactedController.Update(compactedTransform, 40.96f);
	if (!IsNearlyEqual(compactedTransform, compactedTarget)) {
		Finish(state, false, "Compacted transform history did not preserve its edited endpoint.");
		return;
	}

	context.clearAddedSceneModels();
	context.resetDebugEffects();

	ParticleManager* particleManager = ParticleManager::GetInstance();
	particleManager->SetReturning(false);
	particleManager->SetAutoWindSwitchEnabled(false);
	particleManager->SetWindEnabled(false);
	particleManager->ClearAllParticles();

	state.timePlaybackSmokeOrigin_ = context.objectPlane->GetTransform();
	state.timePlaybackSmokeTarget_ = state.timePlaybackSmokeOrigin_;
	state.timePlaybackSmokeTarget_.translate.x += 4.0f;
	state.timePlaybackSmokeTarget_.translate.y += 1.5f;
	state.timePlaybackSmokeTarget_.rotate.y += 0.75f;
	state.timePlaybackSmokeTarget_.scale = { 1.4f, 0.8f, 1.2f };
	context.objectPlane->GetTransform() = state.timePlaybackSmokeTarget_;
	context.objectPlane->RecordTransformEdit(state.timePlaybackSmokeOrigin_, 2.0f);
	context.objectPlane->SetTransformReturning(true);
	if (!context.objectPlane->IsTransformReturning() ||
		!IsNearlyEqual(context.objectPlane->GetTransformPlaybackDuration(), 2.0f)) {
		Finish(state, false, "Object3d Transform Return did not start.");
	}
}

void DebugTimePlaybackSmoke::Update(
	State& state,
	const Context& context,
	float deltaTime)
{
	if (!state.timePlaybackSmokeEnabled_ || state.timePlaybackSmokeFinished_) {
		return;
	}

	auto fail = [&state](const std::string& message) {
		Finish(state, false, message);
	};
	if (!context.objectPlane || !context.objectAxis ||
		!context.normalObjects || !context.animationObjects ||
		!context.addModel || !context.clearAddedSceneModels) {
		fail("A playback test object was deleted unexpectedly.");
		return;
	}

	ParticleManager* particleManager = ParticleManager::GetInstance();

	switch (state.timePlaybackSmokeStage_) {
	case 0: // Transform Returnを中間位置付近で停止します。
		if (!context.objectPlane->IsTransformReturning()) {
			fail("Transform Return stopped before the midpoint pause test.");
			return;
		}
		if (context.objectPlane->GetTransformPlaybackProgress() > 0.55f) {
			return;
		}
		context.objectPlane->SetTransformReturning(false);
		state.timePlaybackSmokePaused_ = context.objectPlane->GetTransform();
		if (context.objectPlane->IsTransformReturning() || !context.objectPlane->CanMoveTransformForward()) {
			fail("Unchecking Transform Return did not enable Move from the paused position.");
			return;
		}
		state.timePlaybackSmokeStableFrames_ = 0;
		state.timePlaybackSmokeStage_ = 1;
		return;

	case 1: // Return解除中にTransformが停止したままか確認します。
		if (!IsNearlyEqual(context.objectPlane->GetTransform(), state.timePlaybackSmokePaused_)) {
			fail("The transform moved after Return was unchecked.");
			return;
		}
		if (++state.timePlaybackSmokeStableFrames_ < 8) {
			return;
		}
		if (!context.objectPlane->MoveTransformForward()) {
			fail("Move did not start from the paused transform.");
			return;
		}
		state.timePlaybackSmokeStage_ = 2;
		return;

	case 2: // Moveが最後に編集したTransformへ戻るか確認します。
		if (context.objectPlane->IsTransformMovingForward()) {
			return;
		}
		if (!IsNearlyEqual(context.objectPlane->GetTransform(), state.timePlaybackSmokeTarget_)) {
			fail("Move did not reach the transform edited in ImGui.");
			return;
		}
		context.objectPlane->SetTransformReturning(true);
		if (!context.objectPlane->IsTransformReturning()) {
			fail("A second Transform Return did not start.");
			return;
		}
		state.timePlaybackSmokeStage_ = 3;
		return;

	case 3: // Return完了時に元のTransformへ戻り、自動停止するか確認します。
		if (context.objectPlane->IsTransformReturning()) {
			return;
		}
		if (!IsNearlyEqual(context.objectPlane->GetTransform(), state.timePlaybackSmokeOrigin_) ||
			!context.objectPlane->CanMoveTransformForward()) {
			fail("A complete Transform Return did not stop at the original transform.");
			return;
		}
		state.timePlaybackSmokeStage_ = 4;
		return;

	case 4: // 再生中のモデルを繰り返し追加・削除して安全性を確認します。
		if (state.timePlaybackSmokeDeleteIterations_ < 32) {
			if (!context.addModel(state.timePlaybackSmokeModelFile_)) {
				fail("Shelf model could not be added during playback delete stress.");
				return;
			}
			if (context.normalObjects->size() != context.baseNormalObjectCount + 1) {
				fail("Shelf normal model count changed unexpectedly during delete stress.");
				return;
			}
			Object3d* addedObject = context.normalObjects->back().get();
			const Transform beforeEdit = addedObject->GetTransform();
			addedObject->GetTransform().translate.x += 2.0f;
			addedObject->RecordTransformEdit(beforeEdit);
			addedObject->SetTransformReturning(true);
			context.clearAddedSceneModels();
			if (context.normalObjects->size() != context.baseNormalObjectCount ||
				context.animationObjects->size() != context.baseAnimationObjectCount) {
				fail("Deleting an active shelf model did not restore the base model counts.");
				return;
			}
			++state.timePlaybackSmokeDeleteIterations_;
			return;
		}

		context.objectAxis->SetAnimationTime(0.35f);
		context.objectAxis->SetAnimationReturning(true);
		if (!context.objectAxis->IsAnimationReturning()) {
			fail("Animation Return did not start.");
			return;
		}
		state.timePlaybackSmokePreviousAnimationTime_ = context.objectAxis->GetAnimationTime();
		state.timePlaybackSmokeStage_ = 5;
		return;

	case 5: { // Animation時間が0へ戻り、Returnが自動解除されるか確認します。
		const float animationTime = context.objectAxis->GetAnimationTime();
		if (context.objectAxis->IsAnimationReturning()) {
			if (animationTime > state.timePlaybackSmokePreviousAnimationTime_ + 0.001f) {
				fail("Animation time increased while Return was checked.");
				return;
			}
			state.timePlaybackSmokePreviousAnimationTime_ = animationTime;
			return;
		}
		if (!IsNearlyEqual(animationTime, 0.0f)) {
			fail("Animation Return cleared before reaching startup time zero.");
			return;
		}
		state.timePlaybackSmokeStage_ = 6;
		return;
	}

	case 6: // Animationが0到達後に前向き再生へ戻るか確認します。
		if (context.objectAxis->IsAnimationReturning()) {
			fail("Animation Return became checked again after auto-clear.");
			return;
		}
		if (context.objectAxis->GetAnimationTime() <= 0.03f) {
			return;
		}
		particleManager->ClearAllParticles();
		particleManager->SetReturning(false);
		particleManager->EmitCylinderEffect("Cylinder", 1, { 0.0f, 0.0f, 0.0f }, 1.0f);
		{
			ParticlePlaybackSnapshot snapshot{};
			if (!particleManager->GetPlaybackSnapshot("Cylinder", snapshot) ||
				snapshot.count != 1 || !snapshot.isEndless) {
				fail("The endless particle for reverse playback could not be created.");
				return;
			}
			state.timePlaybackSmokeParticleRotation_ = snapshot.transform.rotate.y;
		}
		state.timePlaybackSmokeStageTime_ = 0.0f;
		state.timePlaybackSmokeStage_ = 7;
		return;

	case 7: { // Particleが前向きに動く状態を作ります。
		state.timePlaybackSmokeStageTime_ += deltaTime;
		ParticlePlaybackSnapshot snapshot{};
		if (!particleManager->GetPlaybackSnapshot("Cylinder", snapshot) || snapshot.count != 1) {
			fail("The endless particle disappeared during forward playback.");
			return;
		}
		if (state.timePlaybackSmokeStageTime_ < 0.35f) {
			return;
		}
		if (snapshot.transform.rotate.y <= state.timePlaybackSmokeParticleRotation_ + 0.1f) {
			fail("The particle did not move forward before Return.");
			return;
		}
		state.timePlaybackSmokeParticleRotation_ = snapshot.transform.rotate.y;
		particleManager->SetReturning(true);
		particleManager->EmitCylinderEffect("Cylinder", 1, { 0.0f, 0.0f, 0.0f }, 1.0f);
		ParticlePlaybackSnapshot afterBlockedEmit{};
		if (!particleManager->GetPlaybackSnapshot("Cylinder", afterBlockedEmit) || afterBlockedEmit.count != 1) {
			fail("Particle emission was not paused during Return.");
			return;
		}
		state.timePlaybackSmokeStageTime_ = 0.0f;
		state.timePlaybackSmokeStage_ = 8;
		return;
	}

	case 8: { // Return中に無限Particleが連続して逆再生するか確認します。
		state.timePlaybackSmokeStageTime_ += deltaTime;
		ParticlePlaybackSnapshot snapshot{};
		if (!particleManager->IsReturning() ||
			!particleManager->GetPlaybackSnapshot("Cylinder", snapshot) ||
			snapshot.count != 1 || !snapshot.isEndless) {
			fail("Endless particle Return stopped or deleted a particle unexpectedly.");
			return;
		}
		if (state.timePlaybackSmokeStageTime_ < 2.5f) {
			return;
		}
		if (snapshot.transform.rotate.y >= state.timePlaybackSmokeParticleRotation_ - 0.5f) {
			fail("The particle did not continue moving backward while Return stayed checked.");
			return;
		}
		state.timePlaybackSmokeParticleRotation_ = snapshot.transform.rotate.y;
		particleManager->SetReturning(false);
		state.timePlaybackSmokeStageTime_ = 0.0f;
		state.timePlaybackSmokeStage_ = 9;
		return;
	}

	case 9: { // Return解除後、現在位置から前向き再生へ戻るか確認します。
		state.timePlaybackSmokeStageTime_ += deltaTime;
		ParticlePlaybackSnapshot snapshot{};
		if (particleManager->IsReturning() ||
			!particleManager->GetPlaybackSnapshot("Cylinder", snapshot) || snapshot.count != 1) {
			fail("The particle did not remain available after Return was unchecked.");
			return;
		}
		if (state.timePlaybackSmokeStageTime_ < 0.35f) {
			return;
		}
		if (snapshot.transform.rotate.y <= state.timePlaybackSmokeParticleRotation_ + 0.1f) {
			fail("The particle did not resume forward motion from its reverse position.");
			return;
		}
		particleManager->EmitCylinderEffect("Cylinder", 1, { 0.0f, 0.0f, 0.0f }, 1.0f);
		ParticlePlaybackSnapshot afterResumedEmit{};
		if (!particleManager->GetPlaybackSnapshot("Cylinder", afterResumedEmit) || afterResumedEmit.count != 2) {
			fail("Particle emission did not resume after Return was unchecked.");
			return;
		}
		particleManager->ClearAllParticles();
		particleManager->EmitLightCore("LightCore", 1, { 0.0f, 0.0f, 0.0f }, 1.0f);
		state.timePlaybackSmokeStageTime_ = 0.0f;
		state.timePlaybackSmokeStage_ = 10;
		return;
	}

	case 10: { // 有限Particleを少し進めてから巻き戻す準備をします。
		state.timePlaybackSmokeStageTime_ += deltaTime;
		ParticlePlaybackSnapshot snapshot{};
		if (!particleManager->GetPlaybackSnapshot("LightCore", snapshot) ||
			snapshot.count != 1 || snapshot.isEndless) {
			fail("The finite particle disappeared before its Return test.");
			return;
		}
		if (state.timePlaybackSmokeStageTime_ < 0.25f) {
			return;
		}
		particleManager->SetReturning(true);
		state.timePlaybackSmokeStageTime_ = 0.0f;
		state.timePlaybackSmokeStage_ = 11;
		return;
	}

	case 11: { // 時刻0より前へ戻しても有限Particleが安全に循環するか確認します。
		state.timePlaybackSmokeStageTime_ += deltaTime;
		ParticlePlaybackSnapshot snapshot{};
		if (!particleManager->IsReturning() ||
			!particleManager->GetPlaybackSnapshot("LightCore", snapshot) || snapshot.count != 1) {
			fail("A finite particle was deleted while continuously returning past time zero.");
			return;
		}
		if (state.timePlaybackSmokeStageTime_ < 1.35f) {
			return;
		}
		if (snapshot.currentTime < 0.0f || snapshot.currentTime >= snapshot.lifeTime) {
			fail("Finite particle Return did not wrap its playback time safely.");
			return;
		}
		state.timePlaybackSmokeParticleRotation_ = snapshot.currentTime;
		particleManager->SetReturning(false);
		state.timePlaybackSmokeStageTime_ = 0.0f;
		state.timePlaybackSmokeStage_ = 12;
		return;
	}

	case 12: { // 有限Particleも逆再生位置から前向き再生へ戻るか確認します。
		state.timePlaybackSmokeStageTime_ += deltaTime;
		ParticlePlaybackSnapshot snapshot{};
		if (particleManager->IsReturning() ||
			!particleManager->GetPlaybackSnapshot("LightCore", snapshot) || snapshot.count != 1) {
			fail("The finite particle did not remain available after Return was unchecked.");
			return;
		}
		if (state.timePlaybackSmokeStageTime_ < 0.15f) {
			return;
		}
		if (snapshot.currentTime <= state.timePlaybackSmokeParticleRotation_ + 0.05f) {
			fail("The finite particle did not resume forward from its reverse time.");
			return;
		}
		particleManager->ClearAllParticles();
		Finish(
			state,
			true,
			"OK transform=2sTimeline+pause+move+return deleteStress=32 animation=return+autoResume particle=endless+finiteReturn+resume");
		return;
	}

	default:
		fail("Unknown time playback smoke stage.");
		return;
	}
}


// 自動確認の成否をログへ残し、終了コードで外部の自動実行にも結果を返します。
void DebugTimePlaybackSmoke::Finish(
	State& state,
	bool success,
	const std::string& message)
{
	if (state.timePlaybackSmokeFinished_) {
		return;
	}

	state.timePlaybackSmokeFinished_ = true;
	std::ofstream log(state.timePlaybackSmokeLogPath_, std::ios::app);
	if (log) {
		log << (success ? "SUCCESS: " : "FAILURE: ") << message << '\n';
	}
	PostQuitMessage(success ? 0 : 1);
}
