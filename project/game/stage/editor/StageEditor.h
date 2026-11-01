#pragma once

#include "LevelLoader.h"
#include "StageEditViewport.h"
#include "StageEditHistory.h"
#include "StageLevelEditor.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Engine { class Camera; }
namespace Engine { class CameraController; }
class CarryableMirror;
class FixedMirror;
namespace Engine { class Object3d; }
class Player;
class StageCameraEvents;
class StageMapRuntime;

// Stage1のEdit Viewに必要なUI部品をまとめる、編集専用の窓口です。
// Stage1はゲームデータを所有し、このクラスは編集UIを表示して変更操作を依頼します。
class StageEditor
{
public:
	struct Context
	{
		// Edit View以外では何も表示しないための状態です。
		bool isEditViewActive = false;
		int stageId = 1;
		std::string mapFilePath;
		// JSONと実行中のStageデータはStage1が所有します。
		LevelLoader::LevelData* levelData = nullptr;
		Vector3 playerPosition{};
		bool playerStartFromCsv = false;
		Player* player = nullptr;
		const MyMath::OBB* floorObb = nullptr;
		Engine::Object3d* floor = nullptr;
		Engine::Camera* activeCamera = nullptr;
		Engine::CameraController* cameraController = nullptr;
		const std::vector<std::unique_ptr<FixedMirror>>* fixedMirrors = nullptr;
		FixedMirror* mirrorFloor = nullptr;
		CarryableMirror* carryableMirror = nullptr;
		StageMapRuntime* mapRuntime = nullptr;
		const StageCameraEvents* stageCameraEvents = nullptr;
		// 再構築・保存・再読込はStage1の責任なので、必要な操作だけを受け取ります。
		std::function<bool(bool rebuildRuntimeObjects)> applyLevelData;
		// 照明だけの変更では床や未所持の鏡を再配置しないため、専用の反映操作を使います。
		std::function<void()> applyLighting;
		std::function<bool()> saveLevelData;
		std::function<void()> reloadMap;
	};

	// Model Shelfを読み、Stage1のEdit Viewで使う準備をします。
	void Initialize();
	// Collider確認、Level編集、Lighting編集、Viewport操作を順番に表示します。
	void Draw(Context& context);
	// Scene終了時に、Model ShelfとViewportの選択状態を破棄します。
	void Finalize();
	// 自動再読込の選択は編集UIが所有し、Sceneは更新時に結果だけを参照します。
	bool IsAutoMapReloadEnabled() const { return autoMapReload_; }
	// Scene側で起きた読込・Cameraの結果を、同じ編集UIの通知欄へ表示します。
	void SetReloadStatus(std::string status) { mapReloadStatus_ = std::move(status); }
	// 編集した一枚目の固定鏡を、子Objectも含めたJSONデータへ反映し、保存待ちを表示します。
	// @param levelData Stageが所有する保存前のJSONデータです。
	// @param fixedMirror Edit Viewで変更済みの一枚目の固定鏡です。
	// @param baseYaw Puzzleの進行中の回転量を除いた、JSONへ保存する基準角度です。
	// @return 最初のMirrorタグを見つけて反映した時だけtrueです。
	bool SynchronizeFirstMirror(LevelLoader::LevelData& levelData, const FixedMirror& fixedMirror, float baseYaw);
	// JSONのオブジェクト数が変わった時だけ、編集対象の番号を有効範囲へ戻します。
	void ClampSelectedObjectIndex(std::size_t objectCount);
	// Stage切替時だけInspector・Viewport・Shelfの前Stage選択と履歴を消します。
	void ResetForStage(const LevelLoader::LevelData& levelData);
	// JSON再読込後に旧データの履歴を消し、現在Stageの状態から記録を始めます。
	void ResetHistory(const LevelLoader::LevelData& levelData);

private:
	// 現在のPlayerと通常Cameraの角度、次回のPlayerStartを別々に表示します。
	void DrawPlayerControls(Context& context);
	// 携帯鏡を持つ位置を実行中に試し、C++の初期値へ写せるようにします。
	void DrawMirrorHoldControls(CarryableMirror& mirror);
	// Stage1が所有するColliderを、Edit View上へ確認用ワイヤーとして描画します。
	void DrawCollision(const Context& context) const;
	// StageLevelEditorへ渡す、Stage1所有データと再構築・保存の窓口を作ります。
	StageLevelEditor::Context MakeLevelEditorContext(Context& context);
	// 再読込で所有データが入れ替わった時だけtrueを返し、古い参照の使用を止めます。
	bool DrawLevelControls(
		Context& context,
		const StageLevelEditor::Context& levelEditorContext);
	// JSON由来のObjectをViewportで選択・移動・回転・拡縮します。
	void DrawViewport(Context& context);
	// Model追加・削除とViewport操作を担当する既存の小さなUI部品です。
	StageLevelEditor levelEditor_{};
	StageEditViewport editViewport_{};
	StageEditHistory editHistory_{};
	// Edit Viewだけが使う状態は、Sceneのゲーム進行データと分けて所有します。
	bool autoMapReload_ = true;
	std::string mapReloadStatus_ = "Not loaded yet.";
	int selectedObjectIndex_ = 0;
};
