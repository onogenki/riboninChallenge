#pragma once

#include "MyMath.h"
#include <memory>
#include <vector>

class CarryableMirror;
class FixedMirror;
class Object3dCommon;
class Player;
class StageHazardLights;

// Stage1の鏡・Laser・Cameraを、環境変数指定時だけ自動確認するDebug用部品です。
class Stage1GameplaySmoke
{
public:
	// 自動確認の開始時に必要な、本編が所有するオブジェクトと固定設定です。
	struct InitializeContext
	{
		Player* player = nullptr;
		CarryableMirror* carryableMirror = nullptr;
		const std::vector<std::unique_ptr<FixedMirror>>* fixedMirrors = nullptr;
		FixedMirror* mirrorFloor = nullptr;
		Vector3 laserOrigin{};
		Vector3 laserDirection{};
		float laserCollisionRadius = 0.0f;
		Vector3 doorLaserOrigin{};
		Vector3 doorLaserDirection{};
		Vector3 doorSwitchPosition{};
		float doorSwitchRadius = 0.0f;
		float largeMirrorBaseYaw = 0.0f;
		float largeMirrorTargetYawOffset = 0.0f;
		// EnemyManagerがEnemyを生成する時に使う共通3D描画設定です。所有しません。
		Object3dCommon* object3dCommon = nullptr;
	};

	// 毎フレーム変わる本編の値だけを、自動確認へ渡します。
	struct UpdateContext
	{
		const StageHazardLights* hazardLights = nullptr;
		// Stage1がPlayerへ渡す障害物一覧です。持てるMirrorが床にある時だけ含まれます。
		const std::vector<MyMath::OBB>* solidObbs = nullptr;
		bool isDoorSwitchReceivingLight = false;
		float doorOpenAmount = 0.0f;
	};

	// 環境変数が有効な時だけ、鏡・Laser・Cameraの確認を準備します。
	void Initialize(const InitializeContext& context);
	// 一フレーム分の本編結果を記録し、終了時にログへ出力します。
	void Update(const UpdateContext& context, float deltaTime);
	// 自動確認が有効ならtrueを返し、Stage1の操作入力を切り替えるために使います。
	bool IsEnabled() const;

private:
	// 自動確認が使う本編オブジェクトへの非所有ポインタです。
	Player* player_ = nullptr;
	CarryableMirror* carryableMirror_ = nullptr;
	const std::vector<std::unique_ptr<FixedMirror>>* fixedMirrors_ = nullptr;
	FixedMirror* mirrorFloor_ = nullptr;
	Vector3 laserOrigin_{};
	Vector3 laserDirection_{};
	float laserCollisionRadius_ = 0.0f;
	Vector3 doorLaserOrigin_{};
	Vector3 doorLaserDirection_{};
	Vector3 doorSwitchPosition_{};
	float doorSwitchRadius_ = 0.0f;
	float largeMirrorBaseYaw_ = 0.0f;
	float largeMirrorTargetYawOffset_ = 0.0f;

	// ---------- 自動確認用テストの途中結果 ----------

	// trueの時だけ、環境変数で指定した自動確認を実行します。
	bool gameplaySmokeEnabled_ = false;
	// 球Collider同士を一行で判定する共通APIの結果を記録します。
	bool gameplaySmokeSphereColliderApi_ = false;
	// EnemyManagerが複数Enemyを生成し、所有・解放できるかを記録します。
	bool gameplaySmokeEnemyManagerSpawn_ = false;
	// Playerが一度でも床へ着地したかを記録します。
	bool gameplaySmokeSawGrounded_ = false;
	// Playerが床の外へ出たかを記録します。
	bool gameplaySmokeLeftFloor_ = false;
	// Playerが床から落下できたかを記録します。
	bool gameplaySmokeFell_ = false;
	// 携帯MirrorをEキー操作で拾えたかを記録します。
	bool gameplaySmokePickedUpMirror_ = false;
	// 携帯MirrorをEキー操作で置けたかを記録します。
	bool gameplaySmokeDroppedMirror_ = false;
	// 床へ置いた携帯Mirrorが、Stage1のPlayer用障害物一覧に含まれたかを記録します。
	bool gameplaySmokeDroppedMirrorIsSolid_ = false;
	// 携帯MirrorがLaserを反射したかを記録します。
	bool gameplaySmokeCarryMirrorReflectedLaser_ = false;
	// 携帯Mirrorの裏面がLaserを反射しないかを記録します。
	bool gameplaySmokeCarryMirrorBackfaceIgnored_ = false;
	// Playerが横を向いた時、携帯Mirrorの位置と反射面も同じ向きへ追従するかを記録します。
	bool gameplaySmokeCarryMirrorFollowsPlayerFacing_ = false;
	// Stage1からPlayerの向きを渡した後も、携帯Mirrorが横向きへ追従するかを記録します。
	bool gameplaySmokeStageMirrorFollowsPlayerFacing_ = false;
	// Stage1経由の追従確認で、鏡を一時的に持ち上げたかを記録します。
	bool gameplaySmokeStageMirrorProbeStarted_ = false;
	// Stage1経由の追従確認を終え、鏡を元の床位置へ戻したかを記録します。
	bool gameplaySmokeStageMirrorProbeFinished_ = false;
	// Stage1経由の追従確認後に戻す、床に置いた鏡の位置です。
	Vector3 gameplaySmokeStageMirrorDropPosition_{};
	// 水平に持つMirrorの操作が正しく切り替わるかを記録します。
	bool gameplaySmokeCarryMirrorHorizontalControl_ = false;
	// 傾けた水平MirrorがLaserの進行方向を変えるかを記録します。
	bool gameplaySmokeCarryMirrorTiltRedirectsLaser_ = false;
	// 持っているMirrorがPlayerへ届くLaserを遮るかを記録します。
	bool gameplaySmokeCarriedMirrorBlocksPlayer_ = false;
	// LaserがPlayerに当たる判定を記録します。
	bool gameplaySmokeLaserHitsPlayer_ = false;
	// 固定MirrorがPlayerの移動を止めるかを記録します。
	bool gameplaySmokeMirrorBlocksPlayer_ = false;
	// 固定Mirrorが反射Textureを更新できたかを記録します。
	bool gameplaySmokeFixedMirrorReflection_ = false;
	// 鏡床がLaserを反射できたかを記録します。
	bool gameplaySmokeMirrorFloorReflectsLaser_ = false;
	// 鏡床が反射Textureを更新できたかを記録します。
	bool gameplaySmokeMirrorFloorReflection_ = false;
	// 危険LightがMirrorで反射したかを記録します。
	bool gameplaySmokeHazardLightReflection_ = false;
	// Doorが初期状態で閉じているかを記録します。
	bool gameplaySmokeDoorStartsClosed_ = false;
	// Door用Laserが斜めに反射できたかを記録します。
	bool gameplaySmokeDoorDiagonalReflection_ = false;
	// 矢印キーによるCamera段階操作を記録します。
	bool gameplaySmokeCameraSteps_ = false;
	// 壁側へCameraを回せないことを記録します。
	bool gameplaySmokeCameraWallBlock_ = false;
	// 壁越しシルエットを使う設定では、壁があってもCamera距離を維持することを記録します。
	bool gameplaySmokeCameraKeepsDistanceBehindWall_ = false;
	// Cameraの回転が一瞬で移動せず補間されるかを記録します。
	bool gameplaySmokeCameraSmooth_ = false;
	// PlayerがCamera側へ動いた時に追従できるかを記録します。
	bool gameplaySmokeCameraBacktracks_ = false;
	// 自動確認を開始してからの経過時間です。
	float gameplaySmokeElapsedTime_ = 0.0f;
	// 自動確認開始時のPlayerの高さです。
	float gameplaySmokeStartY_ = 0.0f;
	// ステージ形状に依存しない落下確認へ使う、開始位置です。
	Vector3 gameplaySmokeStartPosition_{};
	// 床の外に置く落下確認を一度だけ実行したかを記録します。
	bool gameplaySmokeStartedFallProbe_ = false;
	// 自動確認でUpdateしたフレーム数です。
	int gameplaySmokeFrame_ = 0;
};
