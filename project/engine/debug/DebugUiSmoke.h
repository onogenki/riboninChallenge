#pragma once

#include "MyMath.h"
#include "SceneEditor.h"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class GameViewCapture;
class Object3d;
class Sprite;

// Debug画面のモデル棚・プレビュー・画像保存を自動確認するテスト道具です。
// DebugSceneはSceneが所有する一覧と操作関数を渡し、テストの段階・ログ・終了処理はこのクラスが管理します。
class DebugUiSmoke
{
public:
	struct State
	{
		// trueならUI操作の自動確認を実行します。
		bool isEnabled = false;
		// trueならUI操作の自動確認は終了済みです。
		bool isFinished = false;
		// trueならDraw後のGame View保存を待っています。
		bool isPendingCapture = false;
		// 自動確認で処理したフレーム数です。
		int frame = 0;
		// 自動確認のどの段階かを表す番号です。
		int stage = 0;
		// 自動確認で追加するモデル名です。
		std::string modelFile;
		// trueならwalk.gltfだけを単体プレビューして、骨ありModelの描画を確認します。
		bool isAnimationPreviewTest = false;
		// trueなら棚からwalk.gltfを追加し、通常Scene内での描画・Animation更新を記録します。
		bool isAnimationSceneTest = false;
		// 通常Sceneへの自動追加で作られたAnimationモデルの配列番号です。
		size_t animationObjectIndex = 0;
		// 自動確認結果を書き出すログのパスです。
		std::filesystem::path logPath;
	};

	struct PreviewState
	{
		// trueなら3DモデルまたはTextureのプレビュー中です。
		bool isModelPreviewMode = false;
		// trueならTextureだけを見るプレビューモードです。
		bool isTexturePreviewMode = false;
		// 現在プレビューしている3Dモデルです。所有しません。
		Object3d* previewObject = nullptr;
		// 現在プレビューしているTextureのSpriteです。所有しません。
		Sprite* previewSprite = nullptr;
	};

	struct SelectionState
	{
		// trueなら3DモデルがInspector・ギズモの編集対象です。
		bool hasSelectedObject = false;
		// trueなら選択中3DモデルはAnimationモデルです。
		bool selectedObjectIsAnimation = false;
		// 選択中3Dモデルの配列番号です。
		size_t selectedObjectIndex = 0;
		// trueならSpriteがInspector・ギズモの編集対象です。
		bool hasSelectedSprite = false;
		// 選択中Spriteの配列番号です。
		size_t selectedSpriteIndex = 0;
	};

	struct Context
	{
		// モデル棚へ表示する読み込み可能なResource一覧です。所有しません。
		const std::vector<SceneEditor::ShelfEntry>* modelLibrary = nullptr;
		// Sceneが所有する3Dモデル・Sprite一覧です。個数確認にだけ使います。
		std::vector<std::unique_ptr<Object3d>>* normalObjects = nullptr;
		std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		std::vector<std::unique_ptr<Sprite>>* sprites = nullptr;
		// Sceneが最初から持つ編集不可の要素数です。
		size_t baseNormalObjectCount = 0;
		size_t baseAnimationObjectCount = 0;
		size_t baseSpriteCount = 0;
		// DebugSceneが持つプレビュー・選択状態を取得します。
		std::function<PreviewState()> getPreviewState;
		std::function<SelectionState()> getSelectionState;
		// モデル棚と同じ経路でプレビュー・追加・削除を行う関数です。
		std::function<bool(const std::string&)> enterModelPreview;
		std::function<bool(const std::string&)> enterTexturePreview;
		std::function<void()> resetModelPreviewCamera;
		std::function<void()> exitModelPreview;
		std::function<bool(const std::string&)> addModel;
		std::function<bool(const std::string&)> addTexture;
		std::function<void()> clearAddedSceneModels;
		// モデルの見た目から簡易AABBを作る関数です。
		std::function<bool(const Object3d&, MyMath::AABB&)> buildWorldAabb;
		// Draw後にGame Viewを保存する道具です。所有しません。
		GameViewCapture* gameViewCapture = nullptr;
	};

	// 環境変数が有効な時に、UI操作の自動確認を開始します。
	static void Start(State& state, const std::string& timestamp,
		bool isAnimationPreviewTest = false, bool isAnimationSceneTest = false);
	// Update内で、モデル棚・プレビュー・Inspectorの確認を一段階ずつ進めます。
	static void Update(State& state, const Context& context);
	// Draw後に、Game Viewの画像・一枚動画を保存して確認を終了します。
	static void UpdateAfterDraw(State& state, const Context& context);
	// 自動確認が有効かを返します。通常の録画処理との同時実行を防ぐために使います。
	static bool IsEnabled(const State& state);
	// 初期化中・更新中を問わず、同じログ形式で自動確認を終了します。
	static void Finish(State& state, bool success, const std::string& message);
};
