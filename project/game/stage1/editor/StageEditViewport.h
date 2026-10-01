#pragma once

#include "LevelLoader.h"
#include "SceneEditor.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Camera;
class CarryableMirror;
class FixedMirror;
class StageMapRuntime;

// Stage1のJSON配置物を選択・移動・回転・拡縮するEdit View用UI部品です。
// Stage1はLevelDataと実行中モデルを所有し、このクラスはViewportの選択状態だけを所有します。
class StageEditViewport
{
public:
	struct Context
	{
		// Stage1が所有するJSONデータ・Camera・表示Objectです。StageEditViewportは所有しません。
		LevelLoader::LevelData* levelData = nullptr;
		Camera* camera = nullptr;
		Object3d* floor = nullptr;
		// 持てる鏡はStage1が所有します。EditorはJSONの配置を確認・編集するためだけに参照します。
		CarryableMirror* carryableMirror = nullptr;
		const std::vector<std::unique_ptr<FixedMirror>>* fixedMirrors = nullptr;
		StageMapRuntime* mapRuntime = nullptr;
		// LevelData内で現在選択しているObjectの番号です。
		int* selectedObjectIndex = nullptr;
		// ギズモ編集後に、Stage1が実行中モデルとColliderを同期する処理です。
		std::function<bool()> applyEdits;
		// Sceneへ編集結果メッセージを渡す処理です。
		std::function<void(const std::string&)> setStatus;
	};

	// JSON由来の床・鏡・通常モデルをViewportへ並べ、選択・Transform編集を反映します。
	void Draw(const Context& context);
	// Scene終了時に、前回のギズモ選択状態を破棄します。
	void Finalize();

private:
	// 3Dギズモの選択中Objectと操作軸を保持します。
	SceneEditor::ViewportState viewportState_{};
};
