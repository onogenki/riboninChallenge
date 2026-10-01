#pragma once

#include "MyMath.h"
#include <memory>
#include <vector>

class CarryableMirror;
class FixedMirror;
class Object3d;
class StageLightPuzzle;
class StageMapRuntime;

// Stage内の床・壁・Mirror・Door・JSON配置物を、用途別のOBB一覧へまとめる部品です。
// Objectの寿命はStageが所有し、このクラスはCollider一覧の作成結果だけを所有します。
class StageCollisionWorld
{
public:
	struct Context
	{
		// 見た目と判定を共有するStageの床です。所有しません。
		Object3d* floor = nullptr;
		// Playerを止める固定Mirror一覧です。所有しません。
		const std::vector<std::unique_ptr<FixedMirror>>* fixedMirrors = nullptr;
		// 持っていない時だけPlayerを止める小型Mirrorです。所有しません。
		const CarryableMirror* carryableMirror = nullptr;
		// Doorの開閉状態とDoor Colliderを持つPuzzleです。所有しません。
		const StageLightPuzzle* lightPuzzle = nullptr;
		// JSON/CSV由来の通常配置物を持つRuntimeです。所有しません。
		StageMapRuntime* stageMapRuntime = nullptr;
		// DoorモデルがあるStageだけtrueにします。
		bool hasLightDoor = false;
	};

	// Stageの現在のTransformとPuzzle状態から、Player用・Light遮蔽用のOBB一覧を作り直します。
	void Rebuild(const Context& context);
	// JSONで編集した床Colliderのローカル中心と半分の大きさを設定します。
	void SetFloorLocalShape(const Vector3& localCenter, const Vector3& localHalfSize);
	// Edit ViewのCollider表示へ渡す、床の最新OBBを返します。
	const MyMath::OBB& GetFloorObb() const { return floorObb_; }
	// PlayerとCameraの壁回避に使う、床・壁・Mirrorを含むOBB一覧を返します。
	const std::vector<MyMath::OBB>& GetSolidObbs() const { return solidObbs_; }
	// JSONのCameraBoundaryタグを持つ外壁だけを返します。Cameraは室内の壁では縮みません。
	const std::vector<MyMath::OBB>& GetCameraBoundaryObbs() const { return cameraBoundaryObbs_; }
	// Laserを遮る、床・Door・JSON壁だけのOBB一覧を返します。
	const std::vector<MyMath::OBB>& GetLightBlockingObbs() const { return lightBlockingObbs_; }
	// Scene終了時に、前フレームのCollider一覧を残さず消します。
	void Clear();

private:
	// floor.objのローカル座標で使うCollider設定です。
	Vector3 floorLocalHalfSize_{ 10.0f, 1.500001f, 10.0f };
	Vector3 floorColliderLocalCenter_{};
	MyMath::OBB floorObb_{};
	std::vector<MyMath::OBB> solidObbs_;
	std::vector<MyMath::OBB> cameraBoundaryObbs_;
	std::vector<MyMath::OBB> lightBlockingObbs_;
};
