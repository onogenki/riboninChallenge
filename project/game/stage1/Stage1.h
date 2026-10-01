#pragma once

#include "BaseScene.h"
#include "BgmPlayer.h"
#include "FileHotReload.h"
#include "LevelLoader.h"
#include "Laser.h"
#include "MapChipField.h"
#include "Object3d.h"
#include "Object3dCollection.h"
#include "StageCameraEvents.h"
#include "StageCollisionWorld.h"
#include "StageLightPuzzle.h"
#include "StageMapRuntime.h"
#include "StageStart.h"
#include "editor/StageEditor.h"
#include "StageHazardLights.h"
#include "debug/Stage1GameplaySmoke.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

class CameraController;
class CarryableMirror;
class FixedMirror;
class Player;

// 鏡パズル、開始演出、Cameraをまとめて実行するStage1本編のSceneです。
class Stage1 : public BaseScene
{
public:
	// 前方宣言した所有型を安全に生成するため、実装はStage1.cppに置きます。
	Stage1();
	// 前方宣言した所有型を安全に破棄するため、実装はStage1.cppに置きます。
	~Stage1() override;

	// シーンの開始時に、Camera・照明・Stage固有のObjectを作成します。
	void Initialize() override;
	// シーンを終了するときに、Stage1 が所有するモデルを解放します。
	void Finalize() override;
	// 毎フレーム、Player・Camera・ギミック・編集UIを更新します。
	void Update() override;
	// 毎フレーム、部屋・球・鏡の板を画面に描画します。
	void Draw() override;

private:
	// ---------- JSONを実行中のオブジェクトへ変換する補助構造体 ----------

	// 三灯照明は最大3本に固定し、残りのSpotLight枠をLaser演出へ残します。
	static constexpr size_t kStageLightingSpotLightCount = 3;

	// ---------- 初期化の補助関数 ----------

	// DirectX・Camera・Object3d共通設定を、このSceneで使える状態にします。
	void InitializeRenderSystems();
	// JSONを読む前に使う、Stage1の標準照明値を設定します。
	void InitializeDefaultLighting();
	// JSONへ依存しない床・鏡・Laser・Switch・Doorを作成します。
	bool InitializeStageGimmicks();
	// JSONとCSVを最初に読み込み、Colliderを生成してHot Reload監視を開始します。
	// 読込失敗時は予備座標のままStageを続行しないため、成功したかを返します。
	bool InitializeStageMap();
	// Player、通常追従Camera、開始演出Cameraを順に作成します。
	bool InitializePlayerAndCamera();
	// 本編を所有しない自動動作確認へ、必要なStage固有データだけを渡します。
	void InitializeGameplaySmoke();

	// ---------- 更新・描画の補助関数 ----------

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
	// 反射後を含むLaser線分から、床や壁を実際に照らすSpotLight配列を作ります。
	void UpdateLaserSpotLights();
	// ---------- Camera制御 ----------

	// Playerを自動追尾する通常Cameraを更新します。
	void UpdateMainCamera();
	// 開始演出・通常追従・Event Cameraの優先順位で、Stage1のCameraを更新します。
	void UpdateStageCamera(float deltaTime, bool isStageStartPlaying);
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
	bool ReloadStageMap();
	// ゲーム内で編集したLevelDataをJSONへ保存します。
	bool SaveStageMap();
	// 読込済みLevelDataの照明設定を、Stage1全体で共有するLightへ反映します。
	void ApplyStageLighting();
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

	// 床と球です。後で鏡へ映す対象にもなります。
	Object3dCollection sceneObjects_{};
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
	// 見た目とOBBを共有する床モデルです。
	Object3d* floor_ = nullptr;
	// ---------- 通常Cameraと開始演出 ----------

	// Player などの対象を追従する、三人称カメラ専用の操作役です。
	std::unique_ptr<CameraController> cameraController_;
	// 本編開始時だけPlayerとCameraを動かす、開始演出の操作役です。
	std::unique_ptr<StageStart> stageStart_;
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

	// Stage1の外部マップファイルが保存された瞬間を検出します。
	FileHotReload stageMapHotReload_;
	// Stage1のCSVマップチップが保存された瞬間を検出します。
	FileHotReload stageMapChipHotReload_;
	// Stage1のJSONをゲーム内で編集できる形で保持します。
	std::unique_ptr<LevelLoader::LevelData> stageMapData_;
	// stage1.csvから読んだ、B0やP0のマップチップデータです。
	MapChipField stageMapChipField_;
	// JSON・CSVから生成した、床と鏡以外の通常3Dモデルをまとめて管理します。
	StageMapRuntime stageMapRuntime_;
	// JSONから生成するEvent Trigger・Event Camera・Camera Areaをまとめて管理します。
	StageCameraEvents stageCameraEvents_;
	// Hot Reloadウィンドウで選択しているオブジェクト番号です。
	int selectedStageMapObjectIndex_ = 0;
	// Collider確認、Level編集、Viewport操作をまとめるEdit View専用の窓口です。
	StageEditor stageEditor_{};
	// ファイル保存時に自動で再読込するかを切り替えます。
	bool autoStageMapReload_ = true;
	// ImGuiへ表示する、直近の再読込結果です。
	std::string stageMapReloadStatus_ = "Not loaded yet.";
	// ---------- Stage1全体のライト ----------

	// Stage1 内の全モデルで共有する平行光源です。
	Object3d::DirectionalLight directionalLight_{};
	// Stage1 内の全モデルで共有する点光源です。
	Object3d::PointLight pointLight_{};
	// JSONで配置するキー・フィル・バックライトです。先頭から優先して描画へ渡します。
	std::vector<Object3d::SpotLight> stageSpotLights_;
	// Laserの各線分へ対応する、暗い場所を照らす実際のSpotLightです。
	std::array<Object3d::SpotLight, Object3d::kMaximumSpotLightCount> spotLights_{};
	// Stage1の本編データを使って動く、自動確認専用のDebug部品です。
	Stage1GameplaySmoke gameplaySmoke_{};
};
