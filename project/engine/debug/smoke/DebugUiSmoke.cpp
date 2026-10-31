#include "DebugUiSmoke.h"

#include "CaptureManager.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "Sprite.h"
#include "TextureManager.h"
#include <Windows.h>
#include <algorithm>
#include <fstream>

// 環境変数を確認済みのDebugSceneから呼ばれ、UI自動確認の初期状態を作ります。
void DebugUiSmoke::Start(
	State& state,
	const std::string& timestamp,
	bool isAnimationPreviewTest,
	bool isAnimationSceneTest,
	bool isHotReloadTest)
{
	state.isEnabled = true;
	state.isFinished = false;
	state.isPendingCapture = false;
	state.frame = 0;
	state.stage = 0;
	state.modelFile.clear();
	state.isAnimationPreviewTest = isAnimationPreviewTest;
	state.isAnimationSceneTest = isAnimationSceneTest;
	state.isHotReloadTest = isHotReloadTest;
	state.animationObjectIndex = 0;

	std::error_code errorCode;
	std::filesystem::create_directories("logs", errorCode);
	state.logPath = std::filesystem::path("logs") / ("gameplay_ui_smoke_" + timestamp + ".log");
}

// モデル棚・プレビュー・Inspectorの自動確認を、画面初期化を待ちながら一段階ずつ進めます。
void DebugUiSmoke::Update(State& state, const Context& context)
{
	if (!state.isEnabled || state.isFinished) {
		return;
	}
	if (!context.modelLibrary || !context.normalObjects || !context.animationObjects || !context.sprites ||
		!context.getPreviewState || !context.getSelectionState ||
		!context.enterModelPreview || !context.enterTexturePreview || !context.resetModelPreviewCamera ||
		!context.exitModelPreview || !context.addModel || !context.addTexture ||
		!context.clearAddedSceneModels || !context.buildWorldAabb || !context.captureManager) {
		Finish(state, false, "Required Debug UI test context was not initialized.");
		return;
	}

	++state.frame;
	if (state.frame < 6) {
		return;
	}

	if (state.stage == 0) {
		const auto modelIt = std::find_if(
			context.modelLibrary->begin(),
			context.modelLibrary->end(),
			[&state](const SceneEditor::ShelfEntry& entry)
			{
				if (state.isAnimationPreviewTest || state.isAnimationSceneTest) {
					return entry.canLoad && entry.hasAnimation && entry.fileName == "debug/walk.gltf";
				}
				return entry.canLoad && !entry.hasAnimation && !entry.isTexture;
			});
		if (modelIt == context.modelLibrary->end()) {
			Finish(
				state,
				false,
				(state.isAnimationPreviewTest || state.isAnimationSceneTest)
					? "walk.gltf was not found as a loadable animation model."
					: "No loadable non-animation model was found in resources.");
			return;
		}

		state.modelFile = modelIt->fileName;
		if (state.isAnimationSceneTest) {
			// 棚の「Add Selected」と同じ生成・登録・選択を通し、通常のScene描画を保ちます。
			const size_t normalBefore = context.normalObjects->size();
			const size_t animationBefore = context.animationObjects->size();
			if (!context.addModel(state.modelFile) ||
				context.normalObjects->size() != normalBefore ||
				context.animationObjects->size() != animationBefore + 1) {
				Finish(state, false, "walk.gltf was not added to the scene animation list.");
				return;
			}
			state.animationObjectIndex = animationBefore;
			const SelectionState selection = context.getSelectionState();
			if (!selection.hasSelectedObject || !selection.selectedObjectIsAnimation ||
				selection.selectedObjectIndex != state.animationObjectIndex) {
				Finish(state, false, "Scene walk.gltf was not selected for inspector/gizmo editing.");
				return;
			}
			state.stage = 1;
			state.frame = 0;
			return;
		}
		if (!context.enterModelPreview(state.modelFile)) {
			Finish(state, false, "EnterModelPreview failed for " + state.modelFile);
			return;
		}
		const PreviewState previewState = context.getPreviewState();
		if (!previewState.isModelPreviewMode || !previewState.previewObject) {
			Finish(state, false, "Preview mode did not become active.");
			return;
		}

		MyMath::AABB previewAabb{};
		if (!context.buildWorldAabb(*previewState.previewObject, previewAabb)) {
			Finish(state, false, "Preview AABB could not be built.");
			return;
		}
		if (state.isAnimationPreviewTest) {
			// 単体PreviewはほかのModelや2D Spriteに隠れないため、walk.gltfの描画確認に使います。
			state.isPendingCapture = true;
			state.stage = 2;
			state.frame = 0;
			return;
		}

		state.stage = 1;
		state.frame = 0;
		return;
	}

	if (state.isAnimationSceneTest) {
		// Scene本来のUpdateを30回・90回通した画像を残し、静止Previewだけで判断しないようにします。
		if (context.getPreviewState().isModelPreviewMode ||
			state.animationObjectIndex >= context.animationObjects->size()) {
			Finish(state, false, "The scene animation path was replaced or the added model was removed.");
			return;
		}
		const Engine::Object3d* addedObject = context.animationObjects->at(state.animationObjectIndex).get();
		if (!addedObject || addedObject->GetModelName() != state.modelFile ||
			!addedObject->IsSkeletal() || !addedObject->IsAnimating() || addedObject->GetAnimationDuration() <= 0.0f) {
			Finish(state, false, "Added walk.gltf has no active skeletal animation.");
			return;
		}
		// 一枚目の描画後に再読込し、Modelの参照と実行中の材質指定を保って二枚目を描けるか確認します。
		if (state.isHotReloadTest && state.stage == 2 && state.frame == 60) {
			const Engine::Model* modelBeforeReload = addedObject->GetModel();
			Engine::Model* handModel = Engine::ModelManager::GetInstance()->FindModel("debug/sphere.obj");
			if (!modelBeforeReload || !handModel || handModel->GetModelData().materials.empty()) {
				Finish(state, false, "Models required for hot reload were not available.");
				return;
			}
			const float environmentBeforeReload = addedObject->GetEnvironmentCoefficient();
			const std::string handTextureBeforeReload =
				handModel->GetModelData().materials.front().textureFilePath;
			Engine::ModelManager::GetInstance()->ReloadAllLoadedModels();
			Engine::TextureManager::GetInstance()->ReloadAllLoadedTextures();
			if (addedObject->GetModel() != modelBeforeReload ||
				addedObject->GetEnvironmentCoefficient() != environmentBeforeReload ||
				handModel->GetModelData().materials.empty() ||
				handModel->GetModelData().materials.front().textureFilePath != handTextureBeforeReload) {
				Finish(state, false, "Model address or runtime material changed during hot reload.");
				return;
			}
			std::ofstream log(state.logPath, std::ios::app);
			if (log) {
				log << "RELOAD: model and texture requested at frame 60.\n";
			}
		}
		if ((state.stage == 1 && state.frame == 30) || (state.stage == 2 && state.frame == 90)) {
			state.isPendingCapture = true;
		}
		return;
	}

	if (state.stage != 1) {
		return;
	}

	context.resetModelPreviewCamera();
	context.exitModelPreview();
	const PreviewState modelPreviewState = context.getPreviewState();
	if (modelPreviewState.isModelPreviewMode || modelPreviewState.previewObject) {
		Finish(state, false, "ExitModelPreview did not leave preview mode.");
		return;
	}

	const size_t normalBefore = context.normalObjects->size();
	const size_t animationBefore = context.animationObjects->size();
	if (!context.addModel(state.modelFile)) {
		Finish(state, false, "AddModelToScene failed for " + state.modelFile);
		return;
	}
	if (context.normalObjects->size() != normalBefore + 1 ||
		context.animationObjects->size() != animationBefore) {
		Finish(state, false, "Non-animation model did not add to the normal model list.");
		return;
	}

	Engine::Object3d* addedObject = context.normalObjects->back().get();
	const size_t addedObjectIndex = context.normalObjects->size() - 1;
	MyMath::AABB addedAabb{};
	if (!addedObject || !context.buildWorldAabb(*addedObject, addedAabb)) {
		Finish(state, false, "Added model AABB could not be built.");
		return;
	}
	if (addedObject->GetModelName() != state.modelFile) {
		Finish(state, false, "Added model name was not reflected on the inspector target.");
		return;
	}
	if (addedObjectIndex < context.baseNormalObjectCount) {
		Finish(state, false, "Added model was not placed in the editable inspector range.");
		return;
	}
	const SelectionState modelSelectionState = context.getSelectionState();
	if (!modelSelectionState.hasSelectedObject || modelSelectionState.selectedObjectIsAnimation ||
		modelSelectionState.selectedObjectIndex != addedObjectIndex) {
		Finish(state, false, "Added normal model was not selected for inspector/gizmo editing.");
		return;
	}

	// 存在しない画像はSpriteの追加・Preview開始のどちらでも拒否し、現在の選択を残します。
	constexpr const char* kMissingTexture = "Resources/debug/__missing_ui_texture_smoke_7f9ab3__.png";
	if (std::filesystem::exists(kMissingTexture)) {
		Finish(state, false, "The UI texture failure probe path already exists.");
		return;
	}
	const size_t spriteCountBeforeFailure = context.sprites->size();
	const bool addedMissingTexture = context.addTexture(kMissingTexture);
	const bool previewedMissingTexture = context.enterTexturePreview(kMissingTexture);
	const PreviewState previewAfterFailure = context.getPreviewState();
	const SelectionState selectionAfterFailure = context.getSelectionState();
	if (addedMissingTexture || previewedMissingTexture ||
		context.sprites->size() != spriteCountBeforeFailure ||
		previewAfterFailure.isModelPreviewMode || previewAfterFailure.isTexturePreviewMode ||
		selectionAfterFailure.selectedObjectIndex != addedObjectIndex ||
		!selectionAfterFailure.hasSelectedObject || selectionAfterFailure.hasSelectedSprite ||
		Engine::TextureManager::GetInstance()->Contains(kMissingTexture)) {
		Finish(state, false, "A missing UI texture changed the scene, preview, or texture registry.");
		return;
	}
	// 表示中のSpriteも、読めない画像への切替で元の設定を失わないことを確認します。
	Engine::Sprite* existingSprite = context.sprites->empty() ? nullptr : context.sprites->front().get();
	if (!existingSprite) {
		Finish(state, false, "No initialized Sprite was available for the texture change failure probe.");
		return;
	}
	const Vector2 originalSize = existingSprite->GetSize();
	const Vector2 originalTextureSize = existingSprite->GetTextureSize();
	if (existingSprite->SetTexture(kMissingTexture) ||
		existingSprite->GetSize().x != originalSize.x ||
		existingSprite->GetSize().y != originalSize.y ||
		existingSprite->GetTextureSize().x != originalTextureSize.x ||
		existingSprite->GetTextureSize().y != originalTextureSize.y ||
		Engine::TextureManager::GetInstance()->Contains(kMissingTexture)) {
		Finish(state, false, "A missing texture changed an existing Sprite or created a texture registration.");
		return;
	}

	const auto textureIt = std::find_if(
		context.modelLibrary->begin(),
		context.modelLibrary->end(),
		[](const SceneEditor::ShelfEntry& entry)
		{
			return entry.isTexture;
		});
	if (textureIt == context.modelLibrary->end()) {
		Finish(state, false, "No 2D Texture was found in resources.");
		return;
	}
	if (!context.enterTexturePreview(textureIt->textureFilePath)) {
		Finish(state, false, "EnterTexturePreview failed for " + textureIt->textureFilePath);
		return;
	}
	const PreviewState texturePreviewState = context.getPreviewState();
	if (!texturePreviewState.isModelPreviewMode || !texturePreviewState.isTexturePreviewMode ||
		!texturePreviewState.previewSprite) {
		Finish(state, false, "2D Texture preview mode did not become active.");
		return;
	}
	context.resetModelPreviewCamera();
	context.exitModelPreview();
	const PreviewState exitedTexturePreviewState = context.getPreviewState();
	if (exitedTexturePreviewState.isModelPreviewMode || exitedTexturePreviewState.isTexturePreviewMode ||
		exitedTexturePreviewState.previewSprite) {
		Finish(state, false, "ExitModelPreview did not leave 2D Texture preview mode.");
		return;
	}

	const size_t spriteBefore = context.sprites->size();
	if (!context.addTexture(textureIt->textureFilePath)) {
		Finish(state, false, "AddTextureToScene failed for " + textureIt->textureFilePath);
		return;
	}
	const SelectionState spriteSelectionState = context.getSelectionState();
	if (context.sprites->size() != spriteBefore + 1 || !spriteSelectionState.hasSelectedSprite ||
		spriteSelectionState.selectedSpriteIndex != context.sprites->size() - 1 ||
		spriteSelectionState.hasSelectedObject) {
		Finish(state, false, "Added 2D Texture was not selected for Sprite inspector/gizmo editing.");
		return;
	}

	context.clearAddedSceneModels();
	if (context.normalObjects->size() != context.baseNormalObjectCount ||
		context.animationObjects->size() != context.baseAnimationObjectCount ||
		context.sprites->size() != context.baseSpriteCount) {
		Finish(state, false, "ClearAddedSceneModels did not restore base counts.");
		return;
	}

	state.isPendingCapture = true;
	state.stage = 2;
	state.frame = 0;
}

// Draw済みのGame Viewを保存し、作成結果まで確認してUI自動確認を終了します。
void DebugUiSmoke::UpdateAfterDraw(State& state, const Context& context)
{
	if (!state.isEnabled || state.isFinished || !state.isPendingCapture) {
		return;
	}
	if (!context.captureManager) {
		Finish(state, false, "Game View capture was not initialized.");
		return;
	}

	std::vector<unsigned char> pixels;
	int width = 0;
	int height = 0;
	if (!context.captureManager->CapturePixels(pixels, width, height)) {
		Finish(state, false, "CapturePixels failed.");
		return;
	}

	const std::string capturePrefix = state.isAnimationSceneTest
		? "ribonin_challenge_walk_scene_frame" + std::to_string(state.frame) + "_"
		: "ribonin_challenge_ui_smoke_";
	const std::filesystem::path screenshotPath =
		context.captureManager->GetCaptureDirectory("Screenshots") /
		(capturePrefix + context.captureManager->MakeTimestampString() + ".bmp");
	if (!context.captureManager->SavePixelsAsBmp(screenshotPath, pixels, width, height)) {
		Finish(state, false, "SavePixelsAsBmp failed: " + screenshotPath.string());
		return;
	}
	if (state.isAnimationSceneTest) {
		// 保存成功と画面上の可視性は別なので、ログは描画確認用画像を作れた事実だけを記録します。
		const Engine::Object3d& addedObject = *context.animationObjects->at(state.animationObjectIndex);
		const Vector3& position = addedObject.GetTranslate();
		const std::string captureMessage = "scene=DEBUG model=" + state.modelFile +
			" frame=" + std::to_string(state.frame) +
			" animationTime=" + std::to_string(addedObject.GetAnimationTime()) +
			" animationDuration=" + std::to_string(addedObject.GetAnimationDuration()) +
			" position=" + std::to_string(position.x) + "," + std::to_string(position.y) + "," + std::to_string(position.z) +
			" screenshot=" + screenshotPath.string();
		state.isPendingCapture = false;
		if (state.stage == 1) {
			std::ofstream log(state.logPath, std::ios::app);
			if (log) {
				log << "CAPTURE: " << captureMessage << '\n';
			}
			state.stage = 2;
			return;
		}
		Finish(state, true, "Scene animation capture completed; inspect both images. " + captureMessage);
		return;
	}

	const std::filesystem::path videoPath =
		context.captureManager->GetCaptureDirectory("Videos") /
		("ribonin_challenge_ui_smoke_" + context.captureManager->MakeTimestampString() + ".avi");
	if (!context.captureManager->SaveSingleFrameAvi(videoPath, pixels, width, height)) {
		Finish(state, false, "SaveSingleFrameAvi failed: " + videoPath.string());
		return;
	}

	std::error_code errorCode;
	const bool screenshotExists = std::filesystem::exists(screenshotPath, errorCode) && !errorCode;
	errorCode.clear();
	const bool videoExists = std::filesystem::exists(videoPath, errorCode) && !errorCode;
	if (!screenshotExists || !videoExists) {
		Finish(state, false, "Capture files were not created.");
		return;
	}

	state.isPendingCapture = false;
	Finish(state, true, "OK model=" + state.modelFile +
		" inspector=editable" +
		" screenshot=" + screenshotPath.string() +
		" video=" + videoPath.string());
}

// UI自動確認が有効な間だけtrueを返し、通常のキャプチャ更新との競合を防ぎます。
bool DebugUiSmoke::IsEnabled(const State& state)
{
	return state.isEnabled;
}

// 成功・失敗をログへ記録し、Smoke Test専用の実行を終了します。
void DebugUiSmoke::Finish(State& state, bool success, const std::string& message)
{
	if (state.isFinished) {
		return;
	}

	state.isFinished = true;
	std::ofstream log(state.logPath, std::ios::app);
	if (log) {
		log << (success ? "SUCCESS: " : "FAILURE: ") << message << '\n';
	}
	PostQuitMessage(success ? 0 : 1);
}
