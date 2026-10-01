#include "DebugScene.h"
#include "Camera.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "ParticleEmitter.h"
#include "Sprite.h"
#include "../debug/DebugCollisionOverlay.h"
#include "../debug/DebugViewportPlacement.h"
#include <algorithm>

// Debug用コンテンツ部品へ、Sceneが所有する生成先・ECS・選択状態の窓口を渡します。
DebugSceneContent::Context DebugScene::MakeContentContext()
{
	DebugSceneContent::Context context{};
	context.object3dCommon = object3dCommon;
	context.spriteCommon = spriteCommon;
	context.directXCommon = DirectXCommon::GetInstance();
	context.camera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	context.modelLibrary = &debugSceneEditor_.GetEntries();
	context.normalObjects = &normalObjects_;
	context.animationObjects = &animationObjects_;
	context.sprites = &sprites_;
	context.baseNormalObjectCount = levelRuntime_.GetProtectedNormalObjectCount();
	context.baseAnimationObjectCount = baseAnimationObjectCount_;
	context.baseSpriteCount = baseSpriteCount_;
	context.registerModel = [this](Object3d* object, const std::string& sourceFile, bool isAnimated) {
		entityRegistry_.RegisterModel(object, sourceFile, isAnimated);
	};
	context.registerSprite = [this](Sprite* sprite, const std::string& sourceFile) {
		entityRegistry_.RegisterSprite(sprite, sourceFile);
	};
	context.selectObject = [this](bool isAnimation, size_t index) {
		selection_.SelectObject(isAnimation, index, normalObjects_, animationObjects_, entityRegistry_);
	};
	context.selectSprite = [this](size_t index) {
		selection_.SelectSprite(index, sprites_, entityRegistry_);
	};
	context.updateEcsWorld = [this]() {
		entityRegistry_.Synchronize(normalObjects_, animationObjects_, sprites_);
	};
	context.clearObjectSelection = [this]() {
		selection_.ClearObjectSelection();
	};
	context.clearSpriteSelection = [this]() {
		selection_.ClearSpriteSelection();
	};
	return context;
}

// Debug編集画面部品へ、Sceneが所有するデータと操作だけを渡します。
DebugSceneEditor::Context DebugScene::MakeEditorContext()
{
	DebugSceneEditor::Context context{};
	context.directXCommon = DirectXCommon::GetInstance();
	context.camera = cameraManager ? cameraManager->GetActiveCamera() : nullptr;
	context.cameraManager = cameraManager.get();
	context.normalObjects = &normalObjects_;
	context.animationObjects = &animationObjects_;
	context.sprites = &sprites_;
	context.directionalLight = &directionalLight_;
	context.pointLight = &pointLight_;
	context.spotLight = &spotLight_;
	context.protectedNormalObjectCount = levelRuntime_.GetProtectedNormalObjectCount();
	context.protectedAnimationObjectCount = baseAnimationObjectCount_;
	context.protectedSpriteCount = baseSpriteCount_;
	context.selection = &selection_;
	context.entityRegistry = &entityRegistry_;
	context.assetPreview = &assetPreview_;
	context.gameViewCapture = &gameViewCapture_;
	context.emitterTransform = &emitterTransform_;
	context.emitterCircle = emitterCircle_.get();
	context.emitterPlane = emitterPlane_.get();
	context.activeEmitter = &activeEmitter_;
	context.particleEffects = &debugParticleEffects_;
	context.particleEffectPosition = GetParticleEffectPosition();
	context.addModel = [this](const std::string& fileName) {
		return DebugSceneContent::AddModel(MakeContentContext(), fileName);
	};
	context.addTexture = [this](const std::string& textureFilePath) {
		return DebugSceneContent::AddTexture(MakeContentContext(), textureFilePath);
	};
	context.addModelAtDropPosition = [this](const std::string& fileName, float screenX, float screenY) {
		Vector3 spawnPosition{};
		return DebugViewportPlacement::TryGetWorldPosition(
			cameraManager ? cameraManager->GetActiveCamera() : nullptr,
			screenX,
			screenY,
			spawnPosition)
			? DebugSceneContent::AddModel(MakeContentContext(), fileName, spawnPosition)
			: DebugSceneContent::AddModel(MakeContentContext(), fileName);
	};
	context.addTextureAtDropPosition = [this](const std::string& textureFilePath, float screenX, float screenY) {
		Vector2 spritePosition{};
		return DebugViewportPlacement::TryGetSpritePosition(
			DirectXCommon::GetInstance(),
			screenX,
			screenY,
			spritePosition)
			? DebugSceneContent::AddTexture(MakeContentContext(), textureFilePath, spritePosition)
			: DebugSceneContent::AddTexture(MakeContentContext(), textureFilePath);
	};
	context.clearAdded = [this]() {
		DebugSceneContent::ClearAdded(MakeContentContext());
	};
	context.enterPreview = [this](const SceneEditor::ShelfEntry& entry) {
		return entry.isTexture ? EnterTexturePreview(entry.textureFilePath) : EnterModelPreview(entry.fileName);
	};
	context.exitPreview = [this]() {
		ExitModelPreview();
	};
	context.resetPreview = [this]() {
		assetPreview_.Reset(cameraManager ? cameraManager->GetActiveCamera() : nullptr);
	};
	return context;
}

// Debug用Level読込部品へ、Sceneが所有するObject・ECS・選択状態の窓口を渡します。
DebugLevelRuntime::Context DebugScene::MakeLevelRuntimeContext()
{
	DebugLevelRuntime::Context context{};
	context.object3dCommon = object3dCommon;
	context.normalObjects = &normalObjects_;
	context.animationObjects = &animationObjects_;
	context.sprites = &sprites_;
	context.entityRegistry = &entityRegistry_;
	context.selection = &selection_;
	return context;
}

// Debug専用テストへ、DebugSceneが所有するモデル一覧と操作関数だけを渡します。
DebugTimePlaybackSmoke::Context DebugScene::MakeTimePlaybackSmokeContext()
{
	DebugTimePlaybackSmoke::Context context{};
	context.objectPlane = objectPlane_;
	context.objectAxis = debugAnimationPreview_.GetAttachmentSource();
	context.normalObjects = &normalObjects_;
	context.animationObjects = &animationObjects_;
	context.baseNormalObjectCount = levelRuntime_.GetProtectedNormalObjectCount();
	context.baseAnimationObjectCount = baseAnimationObjectCount_;
	context.addModel = [this](const std::string& fileName)
	{
		return DebugSceneContent::AddModel(MakeContentContext(), fileName);
	};
	context.clearAddedSceneModels = [this]()
	{
		DebugSceneContent::ClearAdded(MakeContentContext());
	};
	context.resetDebugEffects = [this]()
	{
		// 自動確認が前回のParticle設定やEmitter選択を引き継がないよう初期化します。
		activeEmitter_ = nullptr;
		debugParticleEffects_.Clear();
	};
	return context;
}

// Debug専用テストへ、DebugSceneが所有する一覧とプレビュー操作だけを渡します。
DebugUiSmoke::Context DebugScene::MakeUiSmokeContext()
{
	DebugUiSmoke::Context context{};
	context.modelLibrary = &debugSceneEditor_.GetEntries();
	context.normalObjects = &normalObjects_;
	context.animationObjects = &animationObjects_;
	context.sprites = &sprites_;
	context.baseNormalObjectCount = levelRuntime_.GetProtectedNormalObjectCount();
	context.baseAnimationObjectCount = baseAnimationObjectCount_;
	context.baseSpriteCount = baseSpriteCount_;
	context.getPreviewState = [this]()
	{
		return DebugUiSmoke::PreviewState{
			assetPreview_.IsActive(),
			assetPreview_.IsTexturePreview(),
			assetPreview_.GetObject(),
			assetPreview_.GetSprite()
		};
	};
	context.getSelectionState = [this]()
	{
		const DebugSceneSelection::ObjectSelection& objectSelection = selection_.GetObjectSelection();
		const DebugSceneSelection::SpriteSelection& spriteSelection = selection_.GetSpriteSelection();
		return DebugUiSmoke::SelectionState{
			objectSelection.hasSelection,
			objectSelection.isAnimationObject,
			objectSelection.index,
			spriteSelection.hasSelection,
			spriteSelection.index
		};
	};
	context.enterModelPreview = [this](const std::string& fileName)
	{
		return EnterModelPreview(fileName);
	};
	context.enterTexturePreview = [this](const std::string& filePath)
	{
		return EnterTexturePreview(filePath);
	};
	context.resetModelPreviewCamera = [this]()
	{
		assetPreview_.Reset(cameraManager ? cameraManager->GetActiveCamera() : nullptr);
	};
	context.exitModelPreview = [this]()
	{
		ExitModelPreview();
	};
	context.addModel = [this](const std::string& fileName)
	{
		return DebugSceneContent::AddModel(MakeContentContext(), fileName);
	};
	context.addTexture = [this](const std::string& filePath)
	{
		return DebugSceneContent::AddTexture(MakeContentContext(), filePath);
	};
	context.clearAddedSceneModels = [this]()
	{
		DebugSceneContent::ClearAdded(MakeContentContext());
	};
	context.buildWorldAabb = [](const Object3d& object, MyMath::AABB& outAabb)
	{
		return DebugCollisionOverlay::BuildWorldAabb(object, outAabb);
	};
	context.gameViewCapture = &gameViewCapture_;
	return context;
}
