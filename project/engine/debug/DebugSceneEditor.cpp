#include "DebugSceneEditor.h"

#include "Camera.h"
#include "CameraManager.h"
#include "DebugAssetPreview.h"
#include "DebugCollisionOverlay.h"
#include "DebugEditOverlay.h"
#include "DebugEditViewport.h"
#include "DebugEntityRegistry.h"
#include "DebugInspectorTabs.h"
#include "DebugParticleEffects.h"
#include "DebugSceneSelection.h"
#include "DirectXCommon.h"
#include "GameViewCapture.h"
#include "ImGuiManager.h"
#include "Object3d.h"
#include "ParticleEmitter.h"
#include <cstddef>

// resourcesを調べ、Debug用Model Shelfの項目を準備します。
void DebugSceneEditor::ScanResources()
{
	modelShelf_.ScanResources();
}

// Edit Viewだけで、Inspector・Shelf・Viewport・重ね表示を順番に描画します。
void DebugSceneEditor::Draw(Context& context)
{
	if (!ImGuiManager::GetInstance()->IsEditViewActive()) {
		return;
	}

	DrawInspector(context);
	DrawModelShelf(context);
	DrawEditViewport(context);
	DrawOverlays(context);
}

// DebugSceneの選択・Light・Particle設定を使い、Inspector全体を構成します。
void DebugSceneEditor::DrawInspector(Context& context)
{
#ifdef USE_IMGUI
	if (!context.normalObjects || !context.animationObjects || !context.sprites ||
		!context.directionalLight || !context.pointLight || !context.spotLight ||
		!context.selection || !context.entityRegistry || !context.gameViewCapture) {
		return;
	}

	const DebugSceneSelection::ObjectSelection& objectSelection =
		context.selection->GetObjectSelection();
	const DebugSceneSelection::SpriteSelection& spriteSelection =
		context.selection->GetSpriteSelection();
	const bool autoSelectSpriteInspector = context.selection->IsSpriteInspectorAutoSelected();
	const bool autoSelectModelInspector = context.selection->IsObjectInspectorAutoSelected();
	SceneEditor::InspectorOptions options{};
	options.sprites = context.sprites;
	options.normalObjects = context.normalObjects;
	options.animationObjects = context.animationObjects;
	options.directionalLight = context.directionalLight;
	options.pointLight = context.pointLight;
	options.spotLight = context.spotLight;
	options.addedSpriteCount = context.sprites->size() > context.protectedSpriteCount
		? context.sprites->size() - context.protectedSpriteCount : 0;
	options.protectedNormalObjectCount = context.protectedNormalObjectCount;
	options.protectedAnimationObjectCount = context.protectedAnimationObjectCount;
	options.protectedSpriteCount = context.protectedSpriteCount;
	options.forcedSpriteIndex = autoSelectSpriteInspector ? static_cast<int>(spriteSelection.index) : -1;
	options.forcedNormalIndex = autoSelectModelInspector && !objectSelection.isAnimationObject
		? static_cast<int>(objectSelection.index) : -1;
	options.forcedAnimationIndex = autoSelectModelInspector && objectSelection.isAnimationObject
		? static_cast<int>(objectSelection.index) : -1;
	options.selectSpriteTab = autoSelectSpriteInspector;
	options.selectModelTab = autoSelectModelInspector;
	options.forceDock = inspectorForceDockFrames_ > 0;
	options.removeSprite = [&context](size_t index) {
		if (!context.directXCommon || index < context.protectedSpriteCount ||
			index >= context.sprites->size()) {
			return;
		}
		context.directXCommon->WaitForGPU();
		context.sprites->erase(context.sprites->begin() + static_cast<std::ptrdiff_t>(index));
		context.selection->ClearSpriteSelection();
	};
	options.onModelRemoved = [&context](bool, size_t) {
		context.selection->ClearObjectSelection();
	};
	options.drawHeader = [&context]() {
		if (context.gameViewCapture->IsRecording()) {
			ImGui::TextColored(
				ImVec4(1.0f, 0.25f, 0.2f, 1.0f),
				"Recording Game View: %.2f sec",
				context.gameViewCapture->GetRecordingTime());
			ImGui::Separator();
		}
	};
	options.drawExtraTabs = [&context]() {
		DebugInspectorTabs::Draw({
			context.emitterTransform,
			context.emitterCircle,
			context.emitterPlane,
			context.activeEmitter,
			context.particleEffects,
			context.particleEffectPosition,
			context.cameraManager,
			&context.entityRegistry->GetWorld(),
			&context.entityRegistry->GetSelectedEntity(),
		});
	};
	SceneEditor::DrawInspector(options);
	if (inspectorForceDockFrames_ > 0) {
		--inspectorForceDockFrames_;
	}
	context.selection->UpdateInspectorAutoSelection();
#else
	static_cast<void>(context);
#endif
}

// Model Shelfが必要とする追加・Previewの操作を、DebugScene所有のコールバックへ接続します。
void DebugSceneEditor::DrawModelShelf(Context& context)
{
	if (!context.normalObjects || !context.animationObjects || !context.sprites ||
		!context.assetPreview) {
		return;
	}

	modelShelf_.Draw({
		(context.normalObjects->size() > context.protectedNormalObjectCount
			? context.normalObjects->size() - context.protectedNormalObjectCount : 0) +
			(context.animationObjects->size() > context.protectedAnimationObjectCount
				? context.animationObjects->size() - context.protectedAnimationObjectCount : 0),
		context.sprites->size() > context.protectedSpriteCount
			? context.sprites->size() - context.protectedSpriteCount : 0,
		context.addModel,
		context.addTexture,
		context.addModelAtDropPosition,
		context.addTextureAtDropPosition,
		context.clearAdded,
		context.exitPreview,
		context.enterPreview,
		&showCollisionDebug_,
		context.assetPreview->IsActive(),
		context.assetPreview->GetDisplayName(),
		context.exitPreview,
		context.resetPreview,
	});
}

// Edit Viewの選択変更を、DebugSceneが所有する選択状態とECSへ反映します。
void DebugSceneEditor::DrawEditViewport(Context& context)
{
	if (!context.normalObjects || !context.animationObjects || !context.sprites ||
		!context.selection || !context.entityRegistry || !context.assetPreview) {
		return;
	}

	const DebugSceneSelection::ObjectSelection& objectSelection =
		context.selection->GetObjectSelection();
	const DebugSceneSelection::SpriteSelection& spriteSelection =
		context.selection->GetSpriteSelection();
	DebugEditViewport::Draw({
		context.camera,
		context.normalObjects,
		context.animationObjects,
		context.sprites,
		context.assetPreview->IsActive(),
		&modelViewportState_,
		&spriteViewportState_,
		objectSelection.hasSelection,
		objectSelection.isAnimationObject,
		objectSelection.index,
		spriteSelection.hasSelection,
		spriteSelection.index,
		[&context](bool isAnimation, size_t index) {
			context.selection->SelectObject(
				isAnimation,
				index,
				*context.normalObjects,
				*context.animationObjects,
				*context.entityRegistry);
		},
		[&context]() { context.selection->ClearObjectSelection(); },
		[&context](size_t index) {
			context.selection->SelectSprite(index, *context.sprites, *context.entityRegistry);
		},
		[&context]() { context.selection->ClearSpriteSelection(); },
	});
}

// ColliderとPreviewの案内表示は、Debug編集画面でだけ重ねて描画します。
void DebugSceneEditor::DrawOverlays(const Context& context) const
{
	if (!context.normalObjects || !context.animationObjects || !context.entityRegistry ||
		!context.assetPreview) {
		return;
	}

	if (showCollisionDebug_) {
		DebugCollisionOverlay::Draw({
			context.normalObjects,
			context.animationObjects,
			context.assetPreview->GetObject(),
			context.assetPreview->IsActive(),
			&context.entityRegistry->GetWorld(),
			context.camera,
		});
	}
	DebugEditOverlay::Draw({
		context.assetPreview->IsActive(),
		context.assetPreview->IsTexturePreview(),
		context.assetPreview->GetDisplayName(),
	});
}

// Smoke TestやPreview開始処理が読む、Shelfの項目を返します。
std::vector<SceneEditor::ShelfEntry>& DebugSceneEditor::GetEntries()
{
	return modelShelf_.GetEntries();
}

// const版のShelf項目取得です。
const std::vector<SceneEditor::ShelfEntry>& DebugSceneEditor::GetEntries() const
{
	return modelShelf_.GetEntries();
}

// Scene終了時に、Shelf・Viewport・Inspectorの表示状態を初期値へ戻します。
void DebugSceneEditor::Finalize()
{
	modelShelf_.Finalize();
	modelViewportState_ = {};
	spriteViewportState_ = {};
	showCollisionDebug_ = true;
	inspectorForceDockFrames_ = 120;
}
