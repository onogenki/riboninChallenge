#pragma once

#include "MyMath.h"
#include "StageLightPuzzle.h"
#include "Vector3.h"
#include <functional>
#include <memory>
#include <vector>

class Camera;
class CarryableMirror;
class FixedMirror;
class Player;
class StageCameraEvents;
class StageMapRuntime;

// Stage1の鏡・Light Puzzleを調整するImGui表示だけを担当するDebug用UI部品です。
// Stage1はゲームデータを所有し、変更後にJSONデータへ同期する判断だけを持ちます。
class StagePuzzleDebugUi
{
public:
	struct MirrorContext
	{
		// 編集する固定鏡です。所有しません。
		FixedMirror* fixedMirror = nullptr;
		// 鏡の値が変わった後、Stage1がJSON用データを同期する処理です。
		std::function<void()> onMirrorChanged;
	};

	struct LightContext
	{
		// Light PuzzleのUIへ渡す本編データです。すべてStage1が所有します。
		CarryableMirror* carryableMirror = nullptr;
		// 編集可能な配置・方向・幅だけを、一組の設定として借ります。
		StageLightPuzzle::Settings* settings = nullptr;
		// 計算済みのPuzzle状態は表示専用の値として受け取り、UIから書き換えません。
		bool isChargeSwitchReceivingLight = false;
		float mirrorCharge = 0.0f;
		bool isLargeMirrorCharged = false;
		float largeMirrorRotationAmount = 0.0f;
		bool isDoorSwitchReceivingLight = false;
		float doorOpenAmount = 0.0f;
	};

	struct CollisionContext
	{
		// 確認するPlayer・床・鏡・追加オブジェクトはStage1が所有します。
		Player* player = nullptr;
		const MyMath::OBB* floorObb = nullptr;
		Camera* activeCamera = nullptr;
		const std::vector<std::unique_ptr<FixedMirror>>* fixedMirrors = nullptr;
		FixedMirror* mirrorFloor = nullptr;
		CarryableMirror* carryableMirror = nullptr;
		const StageMapRuntime* stageMapRuntime = nullptr;
		const StageCameraEvents* stageCameraEvents = nullptr;
	};

	// 固定鏡の位置・回転・大きさを表示し、変更後にStage1の同期処理を呼びます。
	static void DrawMirror(const MirrorContext& context);
	// Laser・Switch・Doorの調整値を表示し、編集後の安全な値へ正規化して変更有無を返します。
	static bool DrawLightPuzzle(const LightContext& context);
	// Stage1のCollider・Event・Camera位置を、Edit View用ワイヤーとして描画します。
	static void DrawCollision(const CollisionContext& context);
};
