#pragma once

#include "BaseScene.h"
#include "BgmPlayer.h"
#include "LevelLoader.h"
#include "Laser.h"
#include "MapChipField.h"
#include "Object3d.h"
#include "Object3dCollection.h"
#include "StageCameraEvents.h"
#include "StageCollisionWorld.h"
#include "StageLightPuzzle.h"
#include "StageLighting.h"
#include "StageMapFileMonitor.h"
#include "StageMapRuntime.h"
#include "StageMapSelection.h"
#include "StageStart.h"
#include "editor/StageEditor.h"
#include "StageHazardLights.h"
#include "debug/Stage1GameplaySmoke.h"
#include <memory>
#include <string>
#include <vector>

class CameraController;
class CarryableMirror;
class FixedMirror;
class Player;
class SkyBox;
class StageTitleOverlay;
class StagePauseOverlay;
class CameraManager;
class Object3dCommon;

// Stage IDに対応したLevelを読み込み、共通のPlayer・Camera・描画を維持したまま進行するSceneです。
class Stage : public BaseScene
{
public:
	// 前方宣言した所有型を安全に生成するため、実装はStage1.cppに置きます。
	explicit Stage(int initialStageId = 1, bool startsWithTemporaryTitle = false);
	// 前方宣言した所有型を安全に破棄するため、実装はStage1.cppに置きます。
	~Stage() override;

	// シーンの開始時に、Camera・照明・Stage固有のObjectを作成します。
	void Initialize() override;
	// シーンを終了するときに、Stage1 が所有するモデルを解放します。
	void Finalize() override;
	// 毎フレーム、Player・Camera・ギミック・編集UIを更新します。
	void Update() override;
	// 毎フレーム、SkyBox・部屋・球・鏡の板を画面に描画します。
	void Draw() override;
	bool IsReadyForTransition() const override { return isStageReady_; }
	// 次の更新で指定IDのJSONへ切り替える要求を予約し、IDが1〜5以外なら受け付けません。
	bool RequestStage(int stageId);
	// 現在読み込んでいるStage IDを返します。
	int GetStageId() const { return stageMapSelection_.GetStageId(); }

private:
	// 新しいStageの読込を成功時だけ確定し、必要ならPlayerを開始位置へ移動します。
	bool LoadStage(int stageId, bool resetPlayerPosition);
	// Updateの先頭で予約されたStage切替を一度だけ実行します。
	void ApplyRequestedStageChange();

	// Stage1専用のCameraを所有し、描画共通設定はManagerから借ります。
	std::unique_ptr<CameraManager> cameraManager_;
	std::unique_ptr<Camera> mainCamera_;
	Object3dCommon* object3dCommon_ = nullptr;

	// ---------- JSONを実行中のオブジェクトへ変換する補助構造体 ----------

	// ---------- 初期化の補助関数 ----------

	// 描画用Light・Camera・Object3d共通設定を準備し、Lightを描けない時は失敗を返します。
	bool InitializeRenderSystems();
	// JSONへ依存しない床・鏡・Laser・Switch・Doorを作成します。
	bool InitializeStageGimmicks();
	// JSONとCSVを最初に読み込み、Colliderを生成してHot Reload監視を開始します。
	// 読込失敗時は予備座標のままStageを続行しないため、成功したかを返します。
	bool InitializeStageMap();
	// Player、通常追従Camera、開始演出Cameraを順に作成します。
	bool InitializePlayerAndCamera();
	// Stage1本編の初期化時に、従来のPlayer登場演出を同じ設定で開始します。
	void BeginStageStart();
	// 本編を所有しない自動動作確認へ、必要なStage固有データだけを渡します。
	void InitializeGameplaySmoke();

	// ---------- 更新・描画の補助関数 ----------

	// Tabで停止し、停止中のBackspace・その他のキーを一か所で判定します。
	bool UpdatePauseState();
	// StageMapRuntimeが所有するJSON配置物を生成します。Camera・LightはUpdateで一括設定します。
	std::unique_ptr<Object3d> CreateRuntimeObject(const std::string& modelName);
	// 開始演出・自動確認・通常入力のいずれかでPlayerを更新し、開始演出中ならtrueを返します。
	bool UpdateStagePlayer(float deltaTime);
	// 通常Cameraを各鏡面で反転し、反射Cameraの位置と回転を更新します。
	void UpdateReflectionCameras();
	// ---------- 鏡・Laserギミック ----------

	// 持てる鏡の拾う・置く処理を更新します。Laser・Switch・Doorの規則はStageLightPuzzleが更新します。
	void UpdateMirrorGameplay();
	// 固定鏡・持てる鏡・鏡床を、Laserが反射できるMirror一覧として返します。
	std::vector<const Mirror*> GetLaserReflectors() const;
	// 一本の危険LightをLaserとして追跡し、反射後の全線分を返します。
	std::vector<LaserSegment> ReflectHazardLightSegments(const std::vector<LaserSegment>& sourceSegments) const;
	// 鏡の更新後に、Laser・Switch・DoorのPuzzle状態を更新します。
	void UpdateLightPuzzle(float deltaTime);
	// ---------- Camera制御 ----------

	// Playerを自動追尾する通常Cameraを更新します。
	void UpdateMainCamera();
	// 開始演出・通常追従・Event Cameraの優先順位で、Stage1のCameraを更新します。
	void UpdateStageCamera(float deltaTime, bool isStageStartPlaying);
	// 固定CameraのTitle中と本編の双方で、同じRenderContextへ描画対象を渡します。
	void PrepareStageRenderObjects();
	// Stage1の最初の描画後に、BGMの読込・開始・フェード更新を行います。
	void UpdateStageBgm(float deltaTime);
	// カメラが現在見ている正面方向を取得します。
	Vector3 GetCameraForward(const Camera& camera) const;
	// ---------- ImGuiの確認・編集UI ----------

	// 鏡・Light Puzzle用のDebug UIへ、本編データとJSON同期処理を渡します。
	void DrawStagePuzzleDebugUi();
	// Edit Viewでだけ、Levelの編集・保存・Collider確認UIをまとめて表示します。
	void DrawStageEditorUi();
	// ---------- stage1.jsonの読込・保存・編集 ----------

	// Stage1の外部マップファイルが保存されたかを確認します。
	void UpdateStageMapHotReload();
	// Stage1の外部マップファイルを読み、成功した場合だけ床と鏡へ反映します。
	bool ReloadStageMap(bool resetPlayerPosition = false);
	// ゲーム内で編集したLevelDataをJSONへ保存します。
	bool SaveStageMap();
	// Player開始位置、持てるMirror位置、StageStart演出設定を読込済みLevelDataから反映します。
	void ApplyStageStartSettings(
		const LevelLoader::ObjectData* playerStartData,
		const LevelLoader::ObjectData* carryableMirrorData,
		const MapChipField::Chip* csvPlayerStartChip);
	// 通常3D配置物を仮のRuntimeへ生成します。成功後の採用はApplyStageMapDataが決めます。
	bool BuildStageRuntime(
		const std::vector<const LevelLoader::ObjectData*>& additionalObjects,
		StageMapRuntime& outRuntime);
	// Editorで変更した通常3D配置物とCamera Eventを、再生成せず反映します。
	void ApplyStageRuntimeEdits(
		const std::vector<const LevelLoader::ObjectData*>& additionalObjects,
		const std::vector<const LevelLoader::ObjectData*>& eventTriggerDataList,
		const std::vector<const LevelLoader::ObjectData*>& eventCameraDataList,
		const std::vector<const LevelLoader::ObjectData*>& cameraAreaDataList);
	// JSONの床TransformとBOX Colliderを、Stage1が所有する床へ反映します。
	void ApplyFloorData(const LevelLoader::ObjectData& floorData);
	// 読み込んだLevelDataを床・鏡・追加オブジェクトへ反映します。
	bool ApplyStageMapData(bool rebuildRuntimeObjects);
	// ---------- Stage1が所有する3Dオブジェクト ----------

	// 床・発射装置・Switch・Doorを所有し、通常画面と鏡の反射描画へ一覧を貸します。
	Object3dCollection sceneObjects_{};
	// 仮Titleと本編が同じ夜空を使い、Scene終了時にSkyBoxを解放します。
	std::unique_ptr<SkyBox> skyBox_;
	// ---------- 鏡の状態 ----------

	// 大型の固定鏡です。複数枚それぞれが反射Cameraと専用Textureを持ちます。
	std::vector<std::unique_ptr<FixedMirror>> fixedMirrors_;
	// 持ち運べず、両面から来たLightを反射する鏡床ギミックです。
	std::unique_ptr<FixedMirror> mirrorFloor_;
	// 通常床とは別に置く、持ち運べない鏡床ギミックの配置です。
	Vector3 mirrorFloorPosition_{ 5.0f, -1.985f, 11.0f };
	float mirrorFloorWidth_ = 4.5f;
	float mirrorFloorHeight_ = 4.5f;
	// 複数の固定鏡を一枚ずつ順番に更新するための番号です。
	size_t reflectionUpdateCursor_ = 0;
	// 景色は映さず、PlayerがEキーで持ち運べるレーザー反射用の小型鏡です。
	std::unique_ptr<CarryableMirror> carryableMirror_;
	// ---------- Laserの状態 ----------

	// 携帯MirrorでCharge Switchへ送る、シアン色の充電用Laserです。
	// Laser・Switch・Doorの進行状態と判定規則を所有するStage専用ギミックです。
	StageLightPuzzle lightPuzzle_{};
	// Charge Laserの発射位置を見分けるために置く、小さな白い球です。
	Object3d* laserEmitter_ = nullptr;
	// 大型Mirrorの回転後にDoor Switchへ送る、オレンジ色の専用Laserです。
	Object3d* doorLaserEmitter_ = nullptr;
	// ---------- 時間制御で動く危険Light ----------
	// 危険Lightの軌道・反射後線分・Player接触・描画をまとめるStage固有ギミックです。
	StageHazardLights hazardLights_{};
	// ---------- Light PuzzleのSwitchとDoor ----------

	// 携帯鏡で反射したLaserを受け、一定時間で大型Mirrorを起動する充電Switchです。
	Object3d* chargeSwitch_ = nullptr;
	// 斜めを向いた大型Mirrorの反射Laserを受け、Doorを開けるSwitchです。
	Object3d* doorSwitch_ = nullptr;
	// Door Switchへ光が当たっている間だけ上へ移動するDoorです。開いたDoorはPlayerの衝突一覧から外します。
	Object3d* lightDoor_ = nullptr;
	// ---------- Player・床・当たり判定 ----------

	// WASD移動とジャンプを行う球のPlayerです。
	std::unique_ptr<Player> player_;
	// stage1.jsonのPlayerStartから読み込んだ、本編開始時のPlayer座標です。
	Vector3 stagePlayerStartPosition_{ 0.0f, -0.8f, 5.0f };
	// PlayerStartがstage1.jsonに書かれていた時だけtrueになります。
	bool hasStagePlayerStart_ = false;
	// 床の見た目です。寿命はsceneObjects_、判定形状はcollisionWorld_が管理します。
	Object3d* floor_ = nullptr;
	// ---------- 通常Cameraと開始演出 ----------

	// Player などの対象を追従する、三人称カメラ専用の操作役です。
	std::unique_ptr<CameraController> cameraController_;
	// 本編開始時だけPlayerとCameraを動かす、開始演出の操作役です。
	std::unique_ptr<StageStart> stageStart_;
	// 本編Cameraの開始位置を保存し、Title中だけ同じmainCameraを固定視点へ動かします。
	const bool startsWithTemporaryTitle_ = false;
	// 必須モデル・マップ・PlayerがそろうまではUpdateとDrawを行いません。
	bool isStageReady_ = false;
	bool isTemporaryTitleActive_ = false;
	// 三つの排他的な状態だけを持ち、停止とScene切替中の入力を混同しません。
	enum class PauseState { kPlaying, kPaused, kLeavingToStageSelect };
	PauseState pauseState_ = PauseState::kPlaying;
	Vector3 stageStartGameplayCameraPosition_{};
	Vector3 stageStartGameplayCameraRotate_{};
	std::unique_ptr<StageTitleOverlay> stageTitleOverlay_;
	// 本編だけが持つTab・Pause案内です。仮TitleのSpriteとは寿命と表示条件を分けます。
	std::unique_ptr<StagePauseOverlay> stagePauseOverlay_;
	// stage1.jsonのstage_startから読み込む、開始演出の調整値です。
	StageStart::Settings stageStartSettings_{};
	// Loading完了後のStage1画面が出てから再生する、Stage1用のループBGMです。
	BgmPlayer stageBgm_{};
	// Stage1の最初の描画完了を確認してからBGMを読み込むためのフラグです。
	bool hasStageFrameBeenDrawn_ = false;
	bool hasStageBgmStarted_ = false;
	// Player用とLaser遮蔽用のOBB一覧を、Stageの現在状態からまとめて作る部品です。
	StageCollisionWorld collisionWorld_{};
	// ---------- stage1.jsonの実行中データ ----------

	// Stage IDと外部ファイルの対応・切替予約は、Sceneの進行処理から分けて所有します。
	StageMapSelection stageMapSelection_{};

	// 選択中StageのJSONとCSVの変更監視を所有し、Updateへ通知します。
	StageMapFileMonitor stageMapFileMonitor_{};
	// Stage1のJSONをゲーム内で編集できる形で保持します。
	std::unique_ptr<LevelLoader::LevelData> stageMapData_;
	// stage1.csvから読んだ、B0やP0のマップチップデータです。
	MapChipField stageMapChipField_;
	// JSON・CSVから生成した、床と鏡以外の通常3Dモデルをまとめて管理します。
	StageMapRuntime stageMapRuntime_;
	// JSONから生成するEvent Trigger・Event Camera・Camera Areaをまとめて管理します。
	StageCameraEvents stageCameraEvents_;
	// Collider確認、Level編集、Viewport操作をまとめるEdit View専用の窓口です。
	StageEditor stageEditor_{};
	// ---------- Stage1全体のライト ----------

	// JSONの三灯とLaser・危険Lightの線分から作る照明値を所有します。
	StageLighting stageLighting_{};
	// Stage1の本編データを使って動く、自動確認専用のDebug部品です。
	Stage1GameplaySmoke gameplaySmoke_{};
};

// 既存の分割実装ファイル名を維持しながら、外部コードがStageへ移行するまでの互換名を残します。
using Stage1 = Stage;
