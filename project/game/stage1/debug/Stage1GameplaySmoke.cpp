#include "Stage1GameplaySmoke.h"

#include "CameraController.h"
#include "CarryableMirror.h"
#include "Collider.h"
#include "Collision.h"
#include "EnemyManager.h"
#include "FixedMirror.h"
#include "Laser.h"
#include "Player.h"
#include "StageHazardLights.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <fstream>
#include <windows.h>

// 数学関数と判定形状は、この実装ファイル内だけで短い名前を使います。
// ヘッダへusing namespaceを置かないため、ほかのクラスへ名前空間の影響は漏れません。
using namespace MyMath;

namespace
{
	// 環境変数を"1"にした時だけ、自動確認を実行します。
	bool IsEnvironmentEnabled(const char* name)
	{
		char* value = nullptr;
		size_t size = 0;
		if (_dupenv_s(&value, &size, name) != 0 || !value) {
			return false;
		}
		const bool enabled = std::string(value) == "1";
		std::free(value);
		return enabled;
	}
	// 開始演出と床外への移動時間を含めた、自動確認の上限時間です。
	constexpr float kGameplaySmokeTimeoutSeconds = 12.0f;
	// 高フレームレート時に秒数より先に終了しないための、安全用フレーム上限です。
	constexpr int kGameplaySmokeMaximumFrameCount = 10000;
}

// 本編が所有する鏡・Player・固定設定を受け取り、自動確認を開始します。
void Stage1GameplaySmoke::Initialize(const InitializeContext& context)
{
	player_ = context.player;
	carryableMirror_ = context.carryableMirror;
	fixedMirrors_ = context.fixedMirrors;
	mirrorFloor_ = context.mirrorFloor;
	laserOrigin_ = context.laserOrigin;
	laserDirection_ = context.laserDirection;
	laserCollisionRadius_ = context.laserCollisionRadius;
	doorLaserOrigin_ = context.doorLaserOrigin;
	doorLaserDirection_ = context.doorLaserDirection;
	doorSwitchPosition_ = context.doorSwitchPosition;
	doorSwitchRadius_ = context.doorSwitchRadius;
	largeMirrorBaseYaw_ = context.largeMirrorBaseYaw;
	largeMirrorTargetYawOffset_ = context.largeMirrorTargetYawOffset;
	gameplaySmokeEnabled_ = IsEnvironmentEnabled("CG2_STAGE1_GAMEPLAY_SMOKE");
	if (!gameplaySmokeEnabled_ || !player_ || !carryableMirror_ || !fixedMirrors_ ||
		!context.object3dCommon) {
		return;
	}

	// GameObjectを作らずに、Colliderだけで重なり・非重なり・無効状態を確認します。
	SphereCollider firstSphereCollider({ 0.0f, 0.0f, 0.0f }, 1.0f);
	SphereCollider overlappingSphereCollider({ 1.5f, 0.0f, 0.0f }, 1.0f);
	SphereCollider separatedSphereCollider({ 3.0f, 0.0f, 0.0f }, 1.0f);
	const bool detectsOverlap = firstSphereCollider.Check(overlappingSphereCollider).isCollision;
	const bool ignoresSeparatedCollider = !firstSphereCollider.Check(separatedSphereCollider).isCollision;
	overlappingSphereCollider.SetEnabled(false);
	const bool ignoresDisabledCollider = !firstSphereCollider.Check(overlappingSphereCollider).isCollision;
	ObbCollider overlappingObbCollider{};
	overlappingObbCollider.SetLocalShape({}, { 1.0f, 1.0f, 1.0f });
	overlappingObbCollider.SyncTransform({ { 1.0f, 1.0f, 1.0f }, {}, { 1.5f, 0.0f, 0.0f } });
	const Collision::CollisionInfo sphereToObbCollision =
		firstSphereCollider.Check(overlappingObbCollider);
	const Collision::CollisionInfo obbToSphereCollision =
		overlappingObbCollider.Check(firstSphereCollider);
	const bool detectsSphereObbOverlap = sphereToObbCollision.isCollision;
	// 呼び出す側を入れ替えても衝突し、押し出す法線だけ反対向きになります。
	const bool detectsObbSphereOverlap =
		obbToSphereCollision.isCollision &&
		Dot(sphereToObbCollision.normal, obbToSphereCollision.normal) < -0.99f;
	ObbCollider secondObbCollider{};
	secondObbCollider.SetLocalShape({}, { 1.0f, 1.0f, 1.0f });
	secondObbCollider.SyncTransform({ { 1.0f, 1.0f, 1.0f }, {}, { 2.5f, 0.0f, 0.0f } });
	const bool detectsObbObbOverlap =
		overlappingObbCollider.Check(secondObbCollider).isCollision;
	// Player利用側ではColliderの取り出しを省き、Player自身へ判定を依頼できます。
	ObbCollider playerProbeCollider{};
	playerProbeCollider.SetLocalShape({}, { 2.0f, 2.0f, 2.0f });
	playerProbeCollider.SyncTransform({
		{ 1.0f, 1.0f, 1.0f },
		{},
		player_->GetPosition(),
	});
	const bool playerChecksObbDirectly =
		player_->CheckCollision(playerProbeCollider).isCollision;
	// Enemyなどの球Colliderも、Playerから直接判定を呼べます。
	SphereCollider playerSphereProbeCollider{};
	playerSphereProbeCollider.SetShape(player_->GetPosition(), 1.0f);
	const bool playerChecksSphereDirectly =
		player_->CheckCollision(playerSphereProbeCollider).isCollision;
	gameplaySmokeSphereColliderApi_ =
		detectsOverlap &&
		ignoresSeparatedCollider &&
		ignoresDisabledCollider &&
		detectsSphereObbOverlap &&
		detectsObbSphereOverlap &&
		detectsObbObbOverlap &&
		playerChecksObbDirectly &&
		playerChecksSphereDirectly;

	// StageがEnemyを直接newせず、Managerへ一覧生成と寿命管理を任せられるか確認します。
	EnemyManager enemyManager;
	enemyManager.Initialize(context.object3dCommon);
	const std::vector<Enemy::SpawnData> enemySpawnDataList{
		{ "debug/sphere.obj", { -2.0f, 0.0f, 0.0f }, 0.5f, false },
		{ "debug/sphere.obj", { 2.0f, 0.0f, 0.0f }, 0.5f, false },
	};
	const std::vector<Enemy*> spawnedEnemies =
		enemyManager.SpawnAll(enemySpawnDataList);
	// よく使う一体生成は、SpawnDataを組み立てずに短い引数だけで呼べます。
	Enemy* simpleSpawnedEnemy =
		enemyManager.Spawn("debug/sphere.obj", { 0.0f, 0.0f, 0.0f }, 0.5f);
	const bool hasExpectedSpawnedEnemies =
		spawnedEnemies.size() == 2 && simpleSpawnedEnemy != nullptr && enemyManager.GetCount() == 3 &&
		enemyManager.GetEnemy(0) == spawnedEnemies[0] &&
		enemyManager.GetEnemy(1) == spawnedEnemies[1] &&
		enemyManager.GetEnemy(2) == simpleSpawnedEnemy;
	// EnemyもPlayerと同じように、自分のColliderを意識せず判定を呼べます。
	if (hasExpectedSpawnedEnemies) {
		spawnedEnemies[1]->SetPosition({ -1.25f, 0.0f, 0.0f });
	}
	const bool enemyChecksEnemyDirectly =
		hasExpectedSpawnedEnemies &&
		spawnedEnemies[0]->CheckCollision(spawnedEnemies[1]->GetCollider()).isCollision;
	// 同じManagerを再利用しても、前のステージのEnemyを残さないことを確認します。
	enemyManager.Initialize(context.object3dCommon);
	const bool initializeClearsEnemies = enemyManager.GetCount() == 0;
	enemyManager.Clear();
	gameplaySmokeEnemyManagerSpawn_ =
		hasExpectedSpawnedEnemies && enemyChecksEnemyDirectly && initializeClearsEnemies &&
		enemyManager.GetCount() == 0;

	const auto& fixedMirrors = *fixedMirrors_;

	gameplaySmokeStartPosition_ = player_->GetPosition();
	gameplaySmokeStartY_ = gameplaySmokeStartPosition_.y;
	const Vector3 mirrorPosition = carryableMirror_->GetMirror().GetCenter();

	// 本編ではJSONに置かれた鏡から開始するため、床に置かれた状態で拾う処理を確認します。
	carryableMirror_->SetCarried(false);
	// 鏡の中心にPlayerがいる条件を渡し、Eキーと同じ拾う処理を直接確認します。
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, true);
	gameplaySmokePickedUpMirror_ = carryableMirror_->IsCarried();
	// 持った状態のままPlayer正面へ移動させてから、反射判定を行います。
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, false);

	// 携帯中の鏡へ正面から光を当て、衝突後に反射線分が作られることを確認します。
	const Mirror& carryMirror = carryableMirror_->GetMirror();
	const Vector3 mirrorNormal = carryMirror.GetNormal();
	Laser carryMirrorProbe;
	carryMirrorProbe.SetOrigin({
		carryMirror.GetCenter().x + mirrorNormal.x * 3.0f,
		carryMirror.GetCenter().y + mirrorNormal.y * 3.0f,
		carryMirror.GetCenter().z + mirrorNormal.z * 3.0f,
	});
	carryMirrorProbe.SetDirection({ -mirrorNormal.x, -mirrorNormal.y, -mirrorNormal.z });
	carryMirrorProbe.SetMaxDistance(8.0f);
	carryMirrorProbe.SetMaxReflectionCount(1);
	carryMirrorProbe.Update({ &carryMirror });
	const std::vector<LaserSegment>& probeSegments = carryMirrorProbe.GetSegments();
	gameplaySmokeCarryMirrorReflectedLaser_ =
		probeSegments.size() >= 2 && probeSegments.front().hitMirror;

	// 表側とは逆から当てたLaserは、携帯Mirrorでは反射しないことを確認します。
	Laser carryMirrorBackfaceProbe;
	carryMirrorBackfaceProbe.SetOrigin({
		carryMirror.GetCenter().x - mirrorNormal.x * 3.0f,
		carryMirror.GetCenter().y - mirrorNormal.y * 3.0f,
		carryMirror.GetCenter().z - mirrorNormal.z * 3.0f,
	});
	carryMirrorBackfaceProbe.SetDirection(mirrorNormal);
	carryMirrorBackfaceProbe.SetMaxDistance(8.0f);
	carryMirrorBackfaceProbe.SetMaxReflectionCount(1);
	carryMirrorBackfaceProbe.Update({ &carryMirror });
	gameplaySmokeCarryMirrorBackfaceIgnored_ =
		carryMirrorBackfaceProbe.GetSegments().size() == 1 &&
		!carryMirrorBackfaceProbe.GetSegments().front().hitMirror &&
		Length({
			carryMirrorBackfaceProbe.GetSegments().front().end.x - carryMirrorBackfaceProbe.GetSegments().front().start.x,
			carryMirrorBackfaceProbe.GetSegments().front().end.y - carryMirrorBackfaceProbe.GetSegments().front().start.y,
			carryMirrorBackfaceProbe.GetSegments().front().end.z - carryMirrorBackfaceProbe.GetSegments().front().start.z,
		}) < 3.10f;

	// Playerが右を向いた後は、鏡の中心と反射面がともに右前方へ移ることを確認します。
	carryableMirror_->Update(1.0f, mirrorPosition, 1.57079633f, false);
	const Vector3 turnedMirrorCenter = carryableMirror_->GetMirror().GetCenter();
	const Vector3 turnedMirrorNormal = carryableMirror_->GetMirror().GetNormal();
	gameplaySmokeCarryMirrorFollowsPlayerFacing_ =
		turnedMirrorCenter.x - mirrorPosition.x > 2.49f &&
		std::abs(turnedMirrorCenter.z - mirrorPosition.z) < 0.02f &&
		turnedMirrorNormal.x > 0.99f;
	// 後続の水平持ちテストは従来と同じ前方向から始めます。
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, false);

	// 短い右クリックを離す操作を二回行い、上向き・下向きの水平Mirrorになることを確認します。
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, true, 0.0f);
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	const bool isHorizontalMirrorFacingUp = carryableMirror_->GetMirror().GetNormal().y > 0.90f;
	const Vector3 horizontalMirrorOffset{
		carryableMirror_->GetMirror().GetCenter().x - mirrorPosition.x,
		carryableMirror_->GetMirror().GetCenter().y - mirrorPosition.y,
		carryableMirror_->GetMirror().GetCenter().z - mirrorPosition.z,
	};
	const bool isHorizontalMirrorFurtherForward =
		std::sqrt(horizontalMirrorOffset.x * horizontalMirrorOffset.x +
			horizontalMirrorOffset.z * horizontalMirrorOffset.z) > 2.50f;
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, true, 0.0f);
	carryableMirror_->Update(0.05f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	const bool isHorizontalMirrorFacingDown = carryableMirror_->GetMirror().GetNormal().y < -0.90f;
	// 下向きの水平Mirrorを前方へ傾けると、真下からのLaserがPlayer正面へ進むことを確認します。
	carryableMirror_->Update(0.50f, mirrorPosition, 0.0f, false, false, 0.0f, true, 100.0f);
	Laser carryMirrorTiltProbe;
	const Mirror& tiltedCarryMirror = carryableMirror_->GetMirror();
	carryMirrorTiltProbe.SetOrigin({
		tiltedCarryMirror.GetCenter().x,
		tiltedCarryMirror.GetCenter().y - 3.0f,
		tiltedCarryMirror.GetCenter().z,
	});
	carryMirrorTiltProbe.SetDirection({ 0.0f, 1.0f, 0.0f });
	carryMirrorTiltProbe.SetMaxDistance(8.0f);
	carryMirrorTiltProbe.SetMaxReflectionCount(1);
	carryMirrorTiltProbe.Update({ &tiltedCarryMirror });
	const std::vector<LaserSegment>& tiltedProbeSegments = carryMirrorTiltProbe.GetSegments();
	gameplaySmokeCarryMirrorTiltRedirectsLaser_ =
		tiltedProbeSegments.size() >= 2 &&
		tiltedProbeSegments.front().hitMirror &&
		tiltedProbeSegments[1].end.z - tiltedProbeSegments[1].start.z > 0.50f;
	// 長押しを離しても、短押し扱いになって表裏が切り替わらないことを確認します。
	carryableMirror_->Update(0.01f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	// 左クリックと同じ操作で、水平状態から通常の縦向きMirrorへ戻します。
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, false, true, 0.0f);
	// 長押しは鏡を反転させず、構えるための操作だけとして扱います。
	carryableMirror_->Update(0.30f, mirrorPosition, 0.0f, false, false, 0.0f, true, 0.0f);
	carryableMirror_->Update(0.01f, mirrorPosition, 0.0f, false, false, 0.0f, false, 0.0f);
	const bool doesLongRightMouseHoldKeepVertical =
		std::abs(carryableMirror_->GetMirror().GetNormal().y) < 0.01f;
	gameplaySmokeCarryMirrorHorizontalControl_ =
		isHorizontalMirrorFacingUp &&
		isHorizontalMirrorFacingDown &&
		isHorizontalMirrorFurtherForward &&
		doesLongRightMouseHoldKeepVertical;

	// 鏡床は上から下へ向かうLightを、下から上へ反射することを確認します。
	if (mirrorFloor_) {
		Laser mirrorFloorProbe;
		const Mirror& mirrorFloor = mirrorFloor_->GetMirror();
		mirrorFloorProbe.SetOrigin({
			mirrorFloor.GetCenter().x,
			mirrorFloor.GetCenter().y + 3.0f,
			mirrorFloor.GetCenter().z,
		});
		mirrorFloorProbe.SetDirection({ 0.0f, -1.0f, 0.0f });
		mirrorFloorProbe.SetMaxDistance(8.0f);
		mirrorFloorProbe.SetMaxReflectionCount(1);
		mirrorFloorProbe.Update({ &mirrorFloor });
		Laser mirrorFloorBackfaceProbe;
		mirrorFloorBackfaceProbe.SetOrigin({
			mirrorFloor.GetCenter().x,
			mirrorFloor.GetCenter().y - 3.0f,
			mirrorFloor.GetCenter().z,
		});
		mirrorFloorBackfaceProbe.SetDirection({ 0.0f, 1.0f, 0.0f });
		mirrorFloorBackfaceProbe.SetMaxDistance(8.0f);
		mirrorFloorBackfaceProbe.SetMaxReflectionCount(1);
		mirrorFloorBackfaceProbe.Update({ &mirrorFloor });
		gameplaySmokeMirrorFloorReflectsLaser_ =
			mirrorFloorProbe.GetSegments().size() >= 2 &&
			mirrorFloorProbe.GetSegments().front().hitMirror &&
			mirrorFloorProbe.GetSegments()[1].end.y > mirrorFloorProbe.GetSegments()[1].start.y &&
			mirrorFloorBackfaceProbe.GetSegments().size() >= 2 &&
			mirrorFloorBackfaceProbe.GetSegments().front().hitMirror &&
			mirrorFloorBackfaceProbe.GetSegments()[1].end.y < mirrorFloorBackfaceProbe.GetSegments()[1].start.y;
	}

	// 斜め回転した大型MirrorがDoor Switchへ光を送れることを確認します。
	if (!fixedMirrors.empty() && fixedMirrors.front()) {
		const Mirror diagonalDoorMirror(
			fixedMirrors.front()->GetMirror().GetCenter(),
			{ std::sin(largeMirrorBaseYaw_ + largeMirrorTargetYawOffset_), 0.0f,
				std::cos(largeMirrorBaseYaw_ + largeMirrorTargetYawOffset_) },
			fixedMirrors.front()->GetMirror().GetWidth(),
			fixedMirrors.front()->GetMirror().GetHeight());
		Laser diagonalDoorLaser;
		diagonalDoorLaser.SetOrigin(doorLaserOrigin_);
		diagonalDoorLaser.SetDirection(doorLaserDirection_);
		diagonalDoorLaser.SetMaxDistance(20.0f);
		diagonalDoorLaser.SetMaxReflectionCount(1);
		diagonalDoorLaser.Update({ &diagonalDoorMirror });
		const Sphere diagonalDoorSwitch{ doorSwitchPosition_, doorSwitchRadius_ };
		gameplaySmokeDoorDiagonalReflection_ =
			diagonalDoorLaser.IsHitSphere(diagonalDoorSwitch, laserCollisionRadius_);
	}

	const auto laserHitsSphere = [&](const Laser& testLaser, const Sphere& sphere) {
		return testLaser.IsHitSphere(sphere, laserCollisionRadius_);
	};
	// 実際に持っている鏡の現在位置と法線を使い、鏡なしなら当たる光が遮られることを確認します。
	const Vector3 carriedMirrorCenter = carryMirror.GetCenter();
	const Vector3 carriedMirrorNormal = carryMirror.GetNormal();
	const Sphere carriedPlayerSphere{
		{
			carriedMirrorCenter.x - carriedMirrorNormal.x * 2.4f,
			carriedMirrorCenter.y - carriedMirrorNormal.y * 2.4f,
			carriedMirrorCenter.z - carriedMirrorNormal.z * 2.4f,
		},
		1.2f,
	};
	const Vector3 carriedTestOrigin{
		carriedMirrorCenter.x + carriedMirrorNormal.x * 3.0f,
		carriedMirrorCenter.y + carriedMirrorNormal.y * 3.0f,
		carriedMirrorCenter.z + carriedMirrorNormal.z * 3.0f,
	};
	const Vector3 carriedTestDirection{
		-carriedMirrorNormal.x,
		-carriedMirrorNormal.y,
		-carriedMirrorNormal.z,
	};
	Laser carriedUnblockedLaser;
	carriedUnblockedLaser.SetOrigin(carriedTestOrigin);
	carriedUnblockedLaser.SetDirection(carriedTestDirection);
	carriedUnblockedLaser.SetMaxDistance(12.0f);
	carriedUnblockedLaser.Update({});
	Laser carriedBlockedLaser;
	carriedBlockedLaser.SetOrigin(carriedTestOrigin);
	carriedBlockedLaser.SetDirection(carriedTestDirection);
	carriedBlockedLaser.SetMaxDistance(12.0f);
	carriedBlockedLaser.SetMaxReflectionCount(1);
	carriedBlockedLaser.Update({ &carryMirror });
	gameplaySmokeCarriedMirrorBlocksPlayer_ =
		laserHitsSphere(carriedUnblockedLaser, carriedPlayerSphere) &&
		carriedBlockedLaser.GetSegments().size() >= 2 &&
		!laserHitsSphere(carriedBlockedLaser, carriedPlayerSphere);
	carryableMirror_->Update(1.0f, mirrorPosition, 0.0f, true);
	gameplaySmokeDroppedMirror_ = !carryableMirror_->IsCarried();

	// 鏡がない時はPlayerへ届き、途中に鏡がある時は入射線が鏡で止まることを確認します。
	const Sphere testPlayerSphere{ { 0.0f, -0.8f, 5.0f }, 1.2f };
	Laser unblockedLaser;
	unblockedLaser.SetOrigin(laserOrigin_);
	unblockedLaser.SetDirection(laserDirection_);
	unblockedLaser.SetMaxDistance(30.0f);
	unblockedLaser.Update({});
	gameplaySmokeLaserHitsPlayer_ = laserHitsSphere(unblockedLaser, testPlayerSphere);

	const Vector3 normalizedLaserDirection = Normalize(laserDirection_);
	const Mirror shieldMirror(
		{
			laserOrigin_.x + normalizedLaserDirection.x * 1.2f,
			laserOrigin_.y + normalizedLaserDirection.y * 1.2f,
			laserOrigin_.z + normalizedLaserDirection.z * 1.2f,
		},
		Multiply(-1.0f, normalizedLaserDirection),
		3.0f,
		3.0f);
	Laser blockedLaser;
	blockedLaser.SetOrigin(laserOrigin_);
	blockedLaser.SetDirection(laserDirection_);
	blockedLaser.SetMaxDistance(30.0f);
	blockedLaser.SetMaxReflectionCount(1);
	blockedLaser.Update({ &shieldMirror });
	gameplaySmokeMirrorBlocksPlayer_ =
		blockedLaser.GetSegments().size() >= 2 &&
		!laserHitsSphere(blockedLaser, testPlayerSphere);

	// 描画用Cameraとは別の小さなControllerを作り、段階操作と壁制限を確認します。
	Camera testCamera;
	CameraController testController;
	testController.Initialize(&testCamera, { 0.0f, 0.0f, 0.0f });
	testController.SetAutoRecenterEnabled(false);

	// 左一段目のCamera候補へ壁を置き、壁側へ回れないことを確認します。
	constexpr float testYaw = 0.52359878f;
	const float testPitch = testController.GetOrbitPitch();
	const float testDistance = testController.GetDistance();
	const Vector3 testFocus = testController.GetFocus();
	const float horizontalDistance = testDistance * std::cos(testPitch);
	const Vector3 blockedCameraPosition{
		testFocus.x - std::sin(testYaw) * horizontalDistance,
		testFocus.y + std::sin(testPitch) * testDistance,
		testFocus.z - std::cos(testYaw) * horizontalDistance,
	};
	OBB testWall{};
	testWall.center = Lerp(testFocus, blockedCameraPosition, 0.5f);
	testWall.orientations[0] = { 1.0f, 0.0f, 0.0f };
	testWall.orientations[1] = { 0.0f, 1.0f, 0.0f };
	testWall.orientations[2] = { 0.0f, 0.0f, 1.0f };
	testWall.size = { 0.75f, 0.75f, 0.75f };
	gameplaySmokeCameraWallBlock_ =
		!testController.TryStepOrbit(1, { testWall }) &&
		testController.GetOrbitStepIndex() == 0;

	// 壁越しシルエットを使うStageでは、壁があってもCameraをPlayer側へ縮めないことを確認します。
	Camera silhouetteCamera;
	CameraController silhouetteController;
	silhouetteController.Initialize(&silhouetteCamera, { 0.0f, 0.0f, 0.0f });
	silhouetteController.SetAutoRecenterEnabled(false);
	silhouetteController.SetWallAvoidanceEnabled(false);
	const Vector3 silhouetteTargetPosition = silhouetteController.GetCameraPosition();
	OBB silhouetteWall{};
	silhouetteWall.center = Lerp(
		silhouetteController.GetFocus(),
		silhouetteTargetPosition,
		0.5f);
	silhouetteWall.orientations[0] = { 1.0f, 0.0f, 0.0f };
	silhouetteWall.orientations[1] = { 0.0f, 1.0f, 0.0f };
	silhouetteWall.orientations[2] = { 0.0f, 0.0f, 1.0f };
	silhouetteWall.size = { 0.75f, 0.75f, 0.75f };
	silhouetteController.Update(
		1.0f / 60.0f,
		{ 0.0f, 0.0f, 0.0f },
		{},
		false,
		{ silhouetteWall });
	const Vector3 silhouettePositionDifference{
		silhouetteController.GetCameraPosition().x - silhouetteTargetPosition.x,
		silhouetteController.GetCameraPosition().y - silhouetteTargetPosition.y,
		silhouetteController.GetCameraPosition().z - silhouetteTargetPosition.z,
	};
	gameplaySmokeCameraKeepsDistanceBehindWall_ =
		Length(silhouettePositionDifference) < 0.0001f;

	// 一段目の直後も現在角度が目標へ瞬間移動せず、途中にあることを確認します。
	const float yawBeforeStep = testController.GetOrbitYaw();
	const bool firstOrbitStep = testController.TryStepOrbit(1, {});
	testController.Update(
		1.0f / 60.0f,
		{ 0.0f, 0.0f, 0.0f },
		{},
		true,
		{});
	const float yawAfterOneFrame = testController.GetOrbitYaw();
	gameplaySmokeCameraSmooth_ =
		firstOrbitStep &&
		yawAfterOneFrame > yawBeforeStep + 0.0001f &&
		yawAfterOneFrame < testYaw - 0.0001f;

	// PlayerがCamera側へ後退しても、Dead Zoneを越えたFocusがすぐ追いつくことを確認します。
	Camera backwardTestCamera;
	CameraController backwardTestController;
	backwardTestController.Initialize(&backwardTestCamera, { 0.0f, 0.0f, 0.0f });
	backwardTestController.SetAutoRecenterEnabled(false);
	backwardTestController.Update(
		0.25f,
		{ 0.0f, 0.0f, -8.0f },
		{ 0.0f, 0.0f, -1.0f },
		false,
		{});
	gameplaySmokeCameraBacktracks_ = backwardTestController.GetFocus().z < -5.0f;

	// 左右を十二回押すと、一回30度のまま360度回り切れることを確認します。
	Camera fullOrbitCamera;
	CameraController fullOrbitController;
	fullOrbitController.Initialize(&fullOrbitCamera, { 0.0f, 0.0f, 0.0f });
	fullOrbitController.SetAutoRecenterEnabled(false);
	bool canCompleteFullOrbit = true;
	for (int stepIndex = 0; stepIndex < 12; ++stepIndex) {
		canCompleteFullOrbit &= fullOrbitController.TryStepOrbit(1, {});
	}
	const bool canReturnFromFullOrbit =
		fullOrbitController.TryStepOrbit(-1, {}) &&
		fullOrbitController.GetOrbitStepIndex() == 11;
	const bool distanceReachedFarLimit =
		testController.TryStepDistance(1, {}) &&
		!testController.TryStepDistance(1, {}) &&
		testController.GetDistanceStepIndex() == 3;
	const bool distanceReachedNearLimit =
		testController.TryStepDistance(-1, {}) &&
		testController.TryStepDistance(-1, {}) &&
		testController.TryStepDistance(-1, {}) &&
		!testController.TryStepDistance(-1, {}) &&
		testController.GetDistanceStepIndex() == 0;
	gameplaySmokeCameraSteps_ =
		canCompleteFullOrbit &&
		canReturnFromFullOrbit &&
		distanceReachedFarLimit &&
		distanceReachedNearLimit;
}

// 自動確認が有効かを返し、Stage1が専用の入力制御を選ぶために使います。
bool Stage1GameplaySmoke::IsEnabled() const
{
	return gameplaySmokeEnabled_;
}

// Stage1の本編結果を集め、環境変数で起動した時だけ自動確認ログを出力します。
void Stage1GameplaySmoke::Update(const UpdateContext& context, float deltaTime)
{
	if (!gameplaySmokeEnabled_ || !player_) {
		return;
	}

	if (!fixedMirrors_ || !context.hazardLights) {
		return;
	}
	const auto& fixedMirrors = *fixedMirrors_;

	++gameplaySmokeFrame_;
	gameplaySmokeElapsedTime_ += (std::max)(deltaTime, 0.0f);
	if (player_->IsGrounded()) {
		gameplaySmokeSawGrounded_ = true;
	}
	for (const auto& fixedMirror : fixedMirrors) {
		if (fixedMirror && fixedMirror->HasReflectionCapture()) {
			gameplaySmokeFixedMirrorReflection_ = true;
			break;
		}
	}
	if (mirrorFloor_ && mirrorFloor_->HasReflectionCapture()) {
		gameplaySmokeMirrorFloorReflection_ = true;
	}
	const auto hasReflectedSegment = [](const std::vector<LaserSegment>& segments) {
		return std::any_of(
			segments.begin(),
			segments.end(),
			[](const LaserSegment& segment) { return segment.hitMirror; });
	};
	gameplaySmokeHazardLightReflection_ =
		gameplaySmokeHazardLightReflection_ ||
		hasReflectedSegment(context.hazardLights->GetCeilingSweepSegments()) ||
		hasReflectedSegment(context.hazardLights->GetHorizontalMoveSegments()) ||
		hasReflectedSegment(context.hazardLights->GetBottomPulseSegments()) ||
		hasReflectedSegment(context.hazardLights->GetOrbitSegments());
	gameplaySmokeDoorStartsClosed_ =
		gameplaySmokeDoorStartsClosed_ ||
		(!context.isDoorSwitchReceivingLight && context.doorOpenAmount <= 0.01f);
	// 置いたMirrorのOBBが、実際にPlayerへ渡す障害物一覧へ含まれるかを確認します。
	// Collider単体の存在だけでなく、StageCollisionWorldへの登録漏れもここで検出できます。
	if (gameplaySmokeDroppedMirror_ && carryableMirror_ &&
		!carryableMirror_->IsCarried() && context.solidObbs) {
		const MyMath::OBB& droppedMirrorObb = carryableMirror_->GetObb();
		gameplaySmokeDroppedMirrorIsSolid_ = std::any_of(
			context.solidObbs->begin(),
			context.solidObbs->end(),
			[&droppedMirrorObb](const MyMath::OBB& solidObb)
			{
				const Vector3 centerDifference{
					solidObb.center.x - droppedMirrorObb.center.x,
					solidObb.center.y - droppedMirrorObb.center.y,
					solidObb.center.z - droppedMirrorObb.center.z,
				};
				const Vector3 sizeDifference{
					solidObb.size.x - droppedMirrorObb.size.x,
					solidObb.size.y - droppedMirrorObb.size.y,
					solidObb.size.z - droppedMirrorObb.size.z,
				};
				return Length(centerDifference) < 0.001f && Length(sizeDifference) < 0.001f;
			});
	}
	// 本編のStage1::UpdateMirrorGameplayを通し、横向きのPlayerへ鏡面と持ち位置が追従するかを確認します。
	// 確認後は元の床位置へ戻し、後続の落下確認へ影響させません。
	if (carryableMirror_ && !gameplaySmokeStageMirrorProbeStarted_ &&
		gameplaySmokeDroppedMirrorIsSolid_ && gameplaySmokeElapsedTime_ < 6.0f &&
		player_->GetFacingYaw() > 1.3f) {
		gameplaySmokeStageMirrorDropPosition_ = carryableMirror_->GetMirror().GetCenter();
		carryableMirror_->SetCarried(true);
		gameplaySmokeStageMirrorProbeStarted_ = true;
	} else if (carryableMirror_ && gameplaySmokeStageMirrorProbeStarted_ &&
		!gameplaySmokeStageMirrorProbeFinished_) {
		const float facingYaw = player_->GetFacingYaw();
		const Vector3 playerForward{ std::sin(facingYaw), 0.0f, std::cos(facingYaw) };
		const Mirror& heldMirror = carryableMirror_->GetMirror();
		const Vector3 heldOffset{
			heldMirror.GetCenter().x - player_->GetPosition().x,
			0.0f,
			heldMirror.GetCenter().z - player_->GetPosition().z,
		};
		const float heldDistance = Length(heldOffset);
		gameplaySmokeStageMirrorFollowsPlayerFacing_ =
			heldDistance > 0.001f &&
			Dot(heldOffset, playerForward) / heldDistance > 0.95f &&
			Dot(heldMirror.GetNormal(), playerForward) > 0.95f;
		if (gameplaySmokeStageMirrorFollowsPlayerFacing_ || gameplaySmokeElapsedTime_ >= 6.5f) {
			carryableMirror_->SetCarried(false);
			carryableMirror_->SetDroppedPosition(gameplaySmokeStageMirrorDropPosition_);
			gameplaySmokeStageMirrorProbeFinished_ = true;
		}
	}
	// 床の端へ歩かせる検証は、Stageの床形状が変わると成立しません。
	// 開始演出後に床から離れた位置へ一度だけ移し、重力と落下だけを独立して確認します。
	if (gameplaySmokeSawGrounded_ &&
		!gameplaySmokeStartedFallProbe_ &&
		gameplaySmokeElapsedTime_ >= 7.0f) {
		player_->SetPosition({
			gameplaySmokeStartPosition_.x + 20.0f,
			gameplaySmokeStartY_,
			gameplaySmokeStartPosition_.z,
		});
		gameplaySmokeStartedFallProbe_ = true;
	}
	if (gameplaySmokeStartedFallProbe_ && !player_->IsGrounded()) {
		gameplaySmokeLeftFloor_ = true;
	}
	if (gameplaySmokeLeftFloor_ &&
		player_->GetPosition().y < gameplaySmokeStartY_ - 1.0f) {
		gameplaySmokeFell_ = true;
	}

	const bool finishedSuccessfully =
		gameplaySmokeFell_ && gameplaySmokeElapsedTime_ >= 3.0f;
	const bool timedOut =
		gameplaySmokeElapsedTime_ >= kGameplaySmokeTimeoutSeconds ||
		gameplaySmokeFrame_ >= kGameplaySmokeMaximumFrameCount;
	if (!finishedSuccessfully && !timedOut) {
		return;
	}

	const bool success =
		gameplaySmokeSphereColliderApi_ &&
		gameplaySmokeEnemyManagerSpawn_ &&
		gameplaySmokePickedUpMirror_ &&
		gameplaySmokeDroppedMirror_ &&
		gameplaySmokeDroppedMirrorIsSolid_ &&
		gameplaySmokeCarryMirrorReflectedLaser_ &&
		gameplaySmokeCarryMirrorBackfaceIgnored_ &&
		gameplaySmokeCarryMirrorFollowsPlayerFacing_ &&
		gameplaySmokeStageMirrorFollowsPlayerFacing_ &&
		gameplaySmokeCarryMirrorHorizontalControl_ &&
		gameplaySmokeCarryMirrorTiltRedirectsLaser_ &&
		gameplaySmokeCarriedMirrorBlocksPlayer_ &&
		gameplaySmokeLaserHitsPlayer_ &&
		gameplaySmokeMirrorBlocksPlayer_ &&
		gameplaySmokeFixedMirrorReflection_ &&
		gameplaySmokeMirrorFloorReflectsLaser_ &&
		gameplaySmokeMirrorFloorReflection_ &&
		gameplaySmokeDoorStartsClosed_ &&
		gameplaySmokeDoorDiagonalReflection_ &&
		gameplaySmokeCameraSteps_ &&
		gameplaySmokeCameraWallBlock_ &&
		gameplaySmokeCameraKeepsDistanceBehindWall_ &&
		gameplaySmokeCameraSmooth_ &&
		gameplaySmokeCameraBacktracks_ &&
		gameplaySmokeSawGrounded_ &&
		gameplaySmokeLeftFloor_ &&
		gameplaySmokeFell_;
	std::ofstream log("logs/stage1_gameplay_smoke.log", std::ios::trunc);
	if (log) {
		log << (success ? "SUCCESS" : "FAILURE")
			<< ": sphereColliderApi=" << gameplaySmokeSphereColliderApi_
			<< " enemyManager=" << gameplaySmokeEnemyManagerSpawn_
			<< " picked=" << gameplaySmokePickedUpMirror_
			<< " dropped=" << gameplaySmokeDroppedMirror_
			<< " droppedMirrorSolid=" << gameplaySmokeDroppedMirrorIsSolid_
			<< " carryLaser=" << gameplaySmokeCarryMirrorReflectedLaser_
			<< " carryBackface=" << gameplaySmokeCarryMirrorBackfaceIgnored_
			<< " carryFollowsPlayerFacing=" << gameplaySmokeCarryMirrorFollowsPlayerFacing_
			<< " stageMirrorFollowsPlayerFacing=" << gameplaySmokeStageMirrorFollowsPlayerFacing_
			<< " carryHorizontal=" << gameplaySmokeCarryMirrorHorizontalControl_
			<< " carryTiltLaser=" << gameplaySmokeCarryMirrorTiltRedirectsLaser_
			<< " carriedMirrorBlocksPlayer=" << gameplaySmokeCarriedMirrorBlocksPlayer_
			<< " laserHitsPlayer=" << gameplaySmokeLaserHitsPlayer_
			<< " mirrorBlocksPlayer=" << gameplaySmokeMirrorBlocksPlayer_
			<< " fixedMirrorReflection=" << gameplaySmokeFixedMirrorReflection_
			<< " mirrorFloorLaser=" << gameplaySmokeMirrorFloorReflectsLaser_
			<< " mirrorFloorReflection=" << gameplaySmokeMirrorFloorReflection_
			<< " hazardReflection=" << gameplaySmokeHazardLightReflection_
			<< " doorStartsClosed=" << gameplaySmokeDoorStartsClosed_
			<< " doorDiagonal=" << gameplaySmokeDoorDiagonalReflection_
			<< " cameraSteps=" << gameplaySmokeCameraSteps_
			<< " cameraWall=" << gameplaySmokeCameraWallBlock_
			<< " cameraKeepsDistance=" << gameplaySmokeCameraKeepsDistanceBehindWall_
			<< " cameraSmooth=" << gameplaySmokeCameraSmooth_
			<< " cameraBacktracks=" << gameplaySmokeCameraBacktracks_
			<< " grounded=" << gameplaySmokeSawGrounded_
			<< " fallProbe=" << gameplaySmokeStartedFallProbe_
			<< " leftFloor=" << gameplaySmokeLeftFloor_
			<< " fell=" << gameplaySmokeFell_
			<< " startPosition=" << gameplaySmokeStartPosition_.x
			<< ',' << gameplaySmokeStartPosition_.y
			<< ',' << gameplaySmokeStartPosition_.z
			<< " position=" << player_->GetPosition().x
			<< ',' << player_->GetPosition().y
			<< ',' << player_->GetPosition().z
			<< " seconds=" << gameplaySmokeElapsedTime_
			<< '\n';
	}
	gameplaySmokeEnabled_ = false;
	PostQuitMessage(success ? 0 : 1);
}
