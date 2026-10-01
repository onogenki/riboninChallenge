#include "StageReflectionRenderer.h"

#include "Camera.h"
#include "CarryableMirror.h"
#include "DirectXCommon.h"
#include "FixedMirror.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Player.h"
#include "StageHazardLights.h"
#include "StageLightPuzzle.h"

// 一フレームに一枚だけ鏡を描画し、負荷を抑えながら反射Textureを更新します。
void StageReflectionRenderer::DrawOne(Context& context, size_t& reflectionUpdateCursor)
{
	if (!context.directXCommon || !context.object3dCommon || !context.activeCamera ||
		!context.sceneObjects || !context.stageMapRuntime || !context.fixedMirrors ||
		!context.lightPuzzle || !context.hazardLights) {
		return;
	}

	std::vector<FixedMirror*> reflectionMirrors;
	reflectionMirrors.reserve(context.fixedMirrors->size() + 1);
	for (const std::unique_ptr<FixedMirror>& fixedMirror : *context.fixedMirrors) {
		if (fixedMirror) {
			reflectionMirrors.push_back(fixedMirror.get());
		}
	}
	if (context.mirrorFloor) {
		reflectionMirrors.push_back(context.mirrorFloor);
	}
	if (reflectionMirrors.empty()) {
		return;
	}

	const size_t updateCount = reflectionMirrors.size();
	for (size_t attempt = 0; attempt < updateCount; ++attempt) {
		const size_t mirrorIndex = reflectionUpdateCursor % updateCount;
		reflectionUpdateCursor = (reflectionUpdateCursor + 1) % updateCount;
		FixedMirror* fixedMirror = reflectionMirrors[mirrorIndex];
		if (!fixedMirror || !fixedMirror->IsReady()) {
			continue;
		}

		Camera& reflectionCamera = fixedMirror->GetReflectionCamera();
		fixedMirror->BeginReflection(context.directXCommon->GetDepthStencilViewHandle());
		context.object3dCommon->SetCommonDrawSetting();

		// 鏡面同士の無限反射は行わず、通常Object・JSON配置物・Playerだけを描画します。
		for (const std::unique_ptr<Object3d>& object : *context.sceneObjects) {
			if (!object) {
				continue;
			}
			object->UpdateCameraForDraw(&reflectionCamera);
			object->Draw();
		}
		context.stageMapRuntime->UpdateCameraForDraw(&reflectionCamera);
		context.stageMapRuntime->Draw();
		if (context.carryableMirror) {
			context.carryableMirror->GetObject().UpdateCameraForDraw(&reflectionCamera);
			context.carryableMirror->GetObject().Draw();
		}
		if (context.player) {
			context.player->GetObject().UpdateCameraForDraw(&reflectionCamera);
			context.player->GetObject().Draw();
		}
		context.lightPuzzle->Draw(reflectionCamera);
		context.hazardLights->Draw(reflectionCamera);

		fixedMirror->EndReflection();
		RestoreMainCameraMatrices(context);
		break;
	}
}

// 反射Camera用の行列を使ったObjectへ、通常Game Camera用の行列を戻します。
void StageReflectionRenderer::RestoreMainCameraMatrices(const Context& context)
{
	if (!context.activeCamera || !context.sceneObjects || !context.stageMapRuntime ||
		!context.fixedMirrors) {
		return;
	}

	for (const std::unique_ptr<Object3d>& object : *context.sceneObjects) {
		if (object) {
			object->UpdateCameraForDraw(context.activeCamera);
		}
	}
	context.stageMapRuntime->UpdateCameraForDraw(context.activeCamera);
	for (const std::unique_ptr<FixedMirror>& fixedMirror : *context.fixedMirrors) {
		if (fixedMirror) {
			fixedMirror->GetObject().UpdateCameraForDraw(context.activeCamera);
		}
	}
	if (context.mirrorFloor) {
		context.mirrorFloor->GetObject().UpdateCameraForDraw(context.activeCamera);
	}
	if (context.carryableMirror) {
		context.carryableMirror->GetObject().UpdateCameraForDraw(context.activeCamera);
	}
	if (context.player) {
		context.player->GetObject().UpdateCameraForDraw(context.activeCamera);
	}
}
