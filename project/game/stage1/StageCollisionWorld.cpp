#include "StageCollisionWorld.h"

#include "CarryableMirror.h"
#include "Collision.h"
#include "FixedMirror.h"
#include "Object3d.h"
#include "StageLightPuzzle.h"
#include "StageMapRuntime.h"

// 床・Door・Mirror・JSON配置物を、Player用とLight遮蔽用の二つの一覧へ仕分けます。
void StageCollisionWorld::Rebuild(const Context& context)
{
	if (!context.floor) {
		Clear();
		return;
	}

	// 床モデルのTransformから、見た目と同じ位置・回転・拡縮のOBBを作ります。
	floorObb_ = Collision::MakeOBB(
		context.floor->GetTransform(),
		floorColliderLocalCenter_,
		floorLocalHalfSize_);
	solidObbs_ = { floorObb_ };
	cameraBoundaryObbs_.clear();
	lightBlockingObbs_ = { floorObb_ };

	if (context.fixedMirrors) {
		for (const std::unique_ptr<FixedMirror>& fixedMirror : *context.fixedMirrors) {
			if (fixedMirror) {
				solidObbs_.push_back(fixedMirror->GetObb());
			}
		}
	}

	// Doorが開き切るまでは、PlayerとLaserのどちらにも通れない壁として扱います。
	if (context.hasLightDoor && context.lightPuzzle &&
		context.lightPuzzle->GetDoorOpenAmount() < 0.95f) {
		const MyMath::OBB& doorObb = context.lightPuzzle->GetDoorCollider();
		solidObbs_.push_back(doorObb);
		lightBlockingObbs_.push_back(doorObb);
	}

	// 持っているMirrorはPlayer自身と重なるため、置かれている時だけ障害物にします。
	if (context.carryableMirror && !context.carryableMirror->IsCarried()) {
		solidObbs_.push_back(context.carryableMirror->GetObb());
	}

	if (context.stageMapRuntime) {
		context.stageMapRuntime->SyncColliders();
		for (const StageMapRuntime::RuntimeObject& runtimeObject : context.stageMapRuntime->GetObjects()) {
			if (!runtimeObject.visual || !runtimeObject.hasBoxCollider) {
				continue;
			}
			const MyMath::OBB runtimeObb = runtimeObject.GetObb();
			solidObbs_.push_back(runtimeObb);
			lightBlockingObbs_.push_back(runtimeObb);
			// CameraBoundaryは建物の外周だけへ付けるタグです。
			// 室内の壁はここへ入れず、Playerをシルエットで見せる演出を優先します。
			if (runtimeObject.tag == "CameraBoundary") {
				cameraBoundaryObbs_.push_back(runtimeObb);
			}
		}
	}
}

// JSONで編集した床のCollider設定を保存し、次のRebuildから見た目と同じOBBへ反映します。
void StageCollisionWorld::SetFloorLocalShape(
	const Vector3& localCenter,
	const Vector3& localHalfSize)
{
	floorColliderLocalCenter_ = localCenter;
	floorLocalHalfSize_ = localHalfSize;
}

// Scene終了時や床生成失敗時に、古いCollider一覧を使わないよう消去します。
void StageCollisionWorld::Clear()
{
	floorObb_ = {};
	solidObbs_.clear();
	cameraBoundaryObbs_.clear();
	lightBlockingObbs_.clear();
}
