#pragma once

#include "DebugModelShelf.h"
#include "Object3d.h"
#include "SceneEditor.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Camera;
class CameraManager;
class DebugAssetPreview;
class DebugEntityRegistry;
class DebugParticleEffects;
class DebugSceneSelection;
class DirectXCommon;
class GameViewCapture;
class Object3d;
class ParticleEmitter;
class Sprite;

// DebugSceneのInspector・Model Shelf・Edit Viewをまとめて表示する編集専用部品です。
// モデル・Sprite・ECS・Previewの寿命はDebugSceneが所有し、このクラスは編集UIだけを担当します。
class DebugSceneEditor
{
public:
	struct Context
	{
		// DebugSceneが所有する編集対象と、その描画共通設定です。
		DirectXCommon* directXCommon = nullptr;
		Camera* camera = nullptr;
		CameraManager* cameraManager = nullptr;
		std::vector<std::unique_ptr<Object3d>>* normalObjects = nullptr;
		std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		std::vector<std::unique_ptr<Sprite>>* sprites = nullptr;
		Object3d::DirectionalLight* directionalLight = nullptr;
		Object3d::PointLight* pointLight = nullptr;
		Object3d::SpotLight* spotLight = nullptr;
		size_t protectedNormalObjectCount = 0;
		size_t protectedAnimationObjectCount = 0;
		size_t protectedSpriteCount = 0;
		// Sceneが所有する選択・ECS・Preview状態です。
		DebugSceneSelection* selection = nullptr;
		DebugEntityRegistry* entityRegistry = nullptr;
		DebugAssetPreview* assetPreview = nullptr;
		GameViewCapture* gameViewCapture = nullptr;
		// Inspector内のParticle・Effect・Camera・ECSタブが使う値です。
		Transform* emitterTransform = nullptr;
		ParticleEmitter* emitterCircle = nullptr;
		ParticleEmitter* emitterPlane = nullptr;
		ParticleEmitter** activeEmitter = nullptr;
		DebugParticleEffects* particleEffects = nullptr;
		Vector3 particleEffectPosition{};
		// 生成・削除・PreviewはDebugSceneの責任なので、必要な操作だけを受け取ります。
		std::function<bool(const std::string&)> addModel;
		std::function<bool(const std::string&)> addTexture;
		std::function<bool(const std::string&, float, float)> addModelAtDropPosition;
		std::function<bool(const std::string&, float, float)> addTextureAtDropPosition;
		std::function<void()> clearAdded;
		std::function<bool(const SceneEditor::ShelfEntry&)> enterPreview;
		std::function<void()> exitPreview;
		std::function<void()> resetPreview;
	};

	// resourcesを調べ、Debug用Model Shelfの項目を準備します。
	void ScanResources();
	// Edit Viewが有効な時だけ、Inspector・Shelf・Viewport・Collider表示を描画します。
	void Draw(Context& context);
	// Smoke TestやPreview開始処理が読む、現在のShelf項目を返します。
	std::vector<SceneEditor::ShelfEntry>& GetEntries();
	const std::vector<SceneEditor::ShelfEntry>& GetEntries() const;
	// Scene終了時に、編集UIだけが持つ状態を破棄します。
	void Finalize();

private:
	// 3D Object・Sprite・Light・Debug追加タブを含むInspectorを描画します。
	void DrawInspector(Context& context);
	// Model Shelfからの追加・Preview・Dropを、DebugSceneの操作窓口へ渡します。
	void DrawModelShelf(Context& context);
	// 3D・2DのEdit Viewで、Scene所有の選択状態を更新します。
	void DrawEditViewport(Context& context);
	// 有効時だけ、Colliderと編集用Previewの重ね表示を描画します。
	void DrawOverlays(const Context& context) const;
	// Model Shelfの一覧・メッセージを所有します。
	DebugModelShelf modelShelf_{};
	// Edit Viewの3D・2D選択表示だけを所有します。
	SceneEditor::ViewportState modelViewportState_{};
	SceneEditor::SpriteViewportState spriteViewportState_{};
	// trueならColliderワイヤーをGame Viewへ重ねます。
	bool showCollisionDebug_ = true;
	// InspectorをDock位置へ固定する残りフレーム数です。
	int inspectorForceDockFrames_ = 120;
};
