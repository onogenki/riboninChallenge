#pragma once

#include "BaseScene.h"
#include "StageBgmPlayback.h"
#include "LevelLoader.h"
#include "Laser.h"
#include "MapChipField.h"
#include "Object3d.h"
#include "Object3dCollection.h"
#include "StageCameraEvents.h"
#include "StageCollisionWorld.h"
#include "StageLightPuzzle.h"
#include "StageLightDeath.h"
#include "StageLighting.h"
#include "StageLightWarning.h"
#include "StageMapRepository.h"
#include "StageMapRuntime.h"
#include "StageStart.h"
#include "editor/StageEditor.h"
#include "StageHazardLights.h"
#include "debug/Stage1GameplaySmoke.h"
#include <memory>
#include <string>
#include <vector>

namespace Engine { class CameraController; }
class CarryableMirror;
class FixedMirror;
class Player;
namespace Engine { class SkyBox; }
class StageTitleOverlay;
class StagePauseOverlay;
namespace Engine { class CameraManager; }
namespace Engine { class Object3dCommon; }

// Stage IDに対応したLevelを読み込み、共通のPlayer・Camera・描画を維持したまま進行するSceneです。
// 現在の完成データはStage1だけです。Stage2〜5のファイル名定義は今後の実装用です。
class Stage : public BaseScene
{
public:
	// 前方宣言した所有型を安全に生成するため、実装はStage.cppに置きます。
	// @param initialStageId 最初に読み込むStage番号です。現在の通常起動では1を使います。
	// @param startsWithTemporaryTitle trueなら本編のPause案内に代えて仮Title案内を所有します。
	explicit Stage(int initialStageId = 1, bool startsWithTemporaryTitle = false);
	// 前方宣言した所有型を安全に破棄するため、実装はStage.cppに置きます。
	~Stage() override;

	// シーンの開始時に、Camera・照明・Stage固有のObjectを作成します。
	void Initialize() override;
	// シーンを終了するときに、Stageが所有するモデルを解放します。
	void Finalize() override;
	// 毎フレーム、Player・Camera・ギミック・編集UIを更新します。
	void Update() override;
	// 毎フレーム、SkyBox・部屋・球・鏡の板を画面に描画します。
	void Draw() override;
	// 必須資源がそろった時だけ白フェードの解除を許します。
	// @return 初期化が最後まで成功していればtrueです。
	bool IsReadyForTransition() const override { return isStageReady_; }
	// Stage変更を次のUpdateへ予約します。ここではJSON・CSVを読み込みません。
	// @param stageId ファイル名が定義された1〜5の番号です。現時点で完成データがあるのは1だけです。
	// @return 仮Titleでなく、番号の予約を受け付けた時だけtrueです。読込成功は示しません。
	bool RequestStage(int stageId);
	// @return 読込が確定したStage IDです。予約中の番号は含みません。
	int GetStageId() const { return stageMapRepository_.GetStageId(); }

private:
	// 新しいStageの読込を成功時だけ確定し、必要ならPlayerを開始位置へ移動します。
	bool LoadStage(int stageId, bool resetPlayerPosition);
	// Updateの先頭で予約されたStage切替を一度だけ実行します。
	void ApplyRequestedStageChange();

	// 選択中StageのCameraを所有し、描画共通設定はManagerから借ります。
	std::unique_ptr<Engine::CameraManager> cameraManager_;
	std::unique_ptr<Engine::Camera> mainCamera_;
	Engine::Object3dCommon* object3dCommon_ = nullptr;

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
	// CSV・JSONの開始位置がない時も、生成・演出・復帰・再読込で同じ既定座標を使います。
	Vector3 GetStagePlayerStartPosition() const;
	// 本編の初期化時に、選択中StageのPlayer登場演出を開始します。
	void BeginStageStart();
	// 本編を所有しない自動動作確認へ、必要なStage固有データだけを渡します。
	void InitializeGameplaySmoke();

	// ---------- 更新・描画の補助関数 ----------

	// Tabで停止し、停止中のBackspace・その他のキーを一か所で判定します。
	bool UpdatePauseState();
	// 開始演出・自動確認・通常入力のいずれかでPlayerを更新し、開始演出中ならtrueを返します。
	bool UpdateStagePlayer(float deltaTime);
	// 接触後の一秒だけPlayerを上昇・透過させ、復帰かGame Overを決定します。
	void UpdateLightDeath(float deltaTime);
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
	void UpdateLightPuzzle(float deltaTime, bool checkPlayerCollision = true);
	// ---------- Camera制御 ----------

	// Playerを自動追尾する通常Cameraを更新します。
	void UpdateMainCamera();
	// 開始演出・通常追従・Event Cameraの優先順位で、選択中StageのCameraを更新します。
	void UpdateStageCamera(float deltaTime, bool isStageStartPlaying);
	// 固定CameraのTitle中と本編の双方で、同じRenderContextへ描画対象を渡します。
	void PrepareStageRenderObjects();
	// カメラが現在見ている正面方向を取得します。
	Vector3 GetCameraForward(const Engine::Camera& camera) const;
	// ---------- ImGuiの確認・編集UI ----------

	// 鏡・Light Puzzle用のDebug UIへ、本編データとJSON同期処理を渡します。
	void DrawStagePuzzleDebugUi();
	// Edit Viewでだけ、Levelの編集・保存・Collider確認UIをまとめて表示します。
	void DrawStageEditorUi();
	// ---------- 選択中StageのJSON読込・保存・編集 ----------

	// 選択中Stageの外部マップファイルが保存されたかを確認します。
	void UpdateStageMapHotReload();
	// 選択中Stageの外部マップファイルを読み、成功した場合だけ床と鏡へ反映します。
	bool ReloadStageMap(bool resetPlayerPosition = false);
	// ゲーム内で編集したLevelDataをJSONへ保存します。
	bool SaveStageMap();
	// Player開始位置、持てるMirror位置、StageStart演出設定を読込済みLevelDataから反映します。
	void ApplyStageStartSettings(
		const LevelLoader::ObjectData* playerStartData,
		const LevelLoader::ObjectData* carryableMirrorData,
		const MapChipField::Chip* csvPlayerStartChip);
	// Editorで変更した通常3D配置物とCamera Eventを、再生成せず反映します。
	void ApplyStageRuntimeEdits(
		const std::vector<const LevelLoader::ObjectData*>& additionalObjects,
		const std::vector<const LevelLoader::ObjectData*>& eventTriggerDataList,
		const std::vector<const LevelLoader::ObjectData*>& eventCameraDataList,
		const std::vector<const LevelLoader::ObjectData*>& cameraAreaDataList);
	// JSONの床TransformとBOX Colliderを、選択中Stageが所有する床へ反映します。
	void ApplyFloorData(const LevelLoader::ObjectData& floorData);
	// 読み込んだLevelDataを床・鏡・追加オブジェクトへ反映します。
	bool ApplyStageMapData(bool rebuildRuntimeObjects);
	// ---------- 選択中Stageが所有する3Dオブジェクト ----------

	// 床・発射装置・Switch・Doorを所有し、通常画面と鏡の反射描画へ一覧を貸します。
	Engine::Object3dCollection sceneObjects_{};
	// 仮Titleと本編が同じ夜空を使い、Scene終了時にSkyBoxを解放します。
	std::unique_ptr<Engine::SkyBox> skyBox_;
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
	Engine::Object3d* laserEmitter_ = nullptr;
	// 大型Mirrorの回転後にDoor Switchへ送る、オレンジ色の専用Laserです。
	Engine::Object3d* doorLaserEmitter_ = nullptr;
	// ---------- 時間制御で動く危険Light ----------
	// 危険Lightの軌道・反射後線分・Player接触・描画をまとめるStage固有ギミックです。
	StageHazardLights hazardLights_{};
	// Player周囲のAABBと反射後Light線分から、個体別の接近予兆だけを更新します。
	StageLightWarning stageLightWarning_{};
	// ---------- Light PuzzleのSwitchとDoor ----------

	// 携帯鏡で反射したLaserを受け、一定時間で大型Mirrorを起動する充電Switchです。
	Engine::Object3d* chargeSwitch_ = nullptr;
	// 斜めを向いた大型Mirrorの反射Laserを受け、Doorを開けるSwitchです。
	Engine::Object3d* doorSwitch_ = nullptr;
	// Door Switchへ光が当たっている間だけ上へ移動するDoorです。開いたDoorはPlayerの衝突一覧から外します。
	Engine::Object3d* lightDoor_ = nullptr;
	// ---------- Player・床・当たり判定 ----------

	// WASD移動とジャンプを行う球のPlayerです。
	std::unique_ptr<Player> player_;
	// 選択中StageのJSONにあるPlayerStartから読み込んだ、本編開始時のPlayer座標です。
	Vector3 stagePlayerStartPosition_{ 0.0f, -0.8f, 5.0f };
	// PlayerStartが選択中StageのJSONに書かれていた時だけtrueになります。
	bool hasStagePlayerStart_ = false;
	// 危険Lightに触れた回数と一秒の消失演出は、このStage専用部品が所有します。
	StageLightDeath lightDeath_{};
	// Scene切替の予約はStageが行い、消失演出部品にはSceneManagerを持たせません。
	bool isLeavingForGameOver_ = false;
	// 床の見た目です。寿命はsceneObjects_、判定形状はcollisionWorld_が管理します。
	Engine::Object3d* floor_ = nullptr;
	// ---------- 通常Cameraと開始演出 ----------

	// Player などの対象を追従する、三人称カメラ専用の操作役です。
	std::unique_ptr<Engine::CameraController> cameraController_;
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
	// 選択中StageのJSONにあるstage_startから読み込む、開始演出の調整値です。
	StageStart::Settings stageStartSettings_{};
	// 初回描画を待つ条件とVoiceの寿命を一か所で管理します。
	StageBgmPlayback stageBgm_{};
	// Player用とLaser遮蔽用のOBB一覧を、Stageの現在状態からまとめて作る部品です。
	StageCollisionWorld collisionWorld_{};
	// ---------- 選択中Stageの実行中データ ----------

	// Stage ID・JSON/CSV・変更監視をまとめて所有し、Sceneへ読込成否を返します。
	StageMapRepository stageMapRepository_{};
	// JSON・CSVから生成した、床と鏡以外の通常3Dモデルをまとめて管理します。
	StageMapRuntime stageMapRuntime_;
	// JSONから生成するEvent Trigger・Event Camera・Camera Areaをまとめて管理します。
	StageCameraEvents stageCameraEvents_;
	// Collider確認、Level編集、Viewport操作をまとめるEdit View専用の窓口です。
	StageEditor stageEditor_{};
	// ---------- 選択中Stage全体のライト ----------

	// JSONの三灯とLaser・危険Lightの線分から作る照明値を所有します。
	StageLighting stageLighting_{};
	// Stage1の本編データを使って動く、自動確認専用のDebug部品です。
	Stage1GameplaySmoke gameplaySmoke_{};
};

// 既存の分割実装ファイル名を維持しながら、外部コードがStageへ移行するまでの互換名を残します。
using Stage1 = Stage;
