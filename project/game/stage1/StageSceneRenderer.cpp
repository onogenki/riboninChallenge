#include "StageSceneRenderer.h"

#include "Camera.h"
#include "CarryableMirror.h"
#include "FixedMirror.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Player.h"
#include "SceneRenderPipeline.h"
#include "StageHazardLights.h"
#include "StageLightPuzzle.h"

// 本編の通常描画対象を、Mirror Surfaceが最後に重なる順番で表示します。
void StageSceneRenderer::Draw(const Context& context)
{
	if (!context.object3dCommon || !context.activeCamera || !context.sceneObjects ||
		!context.stageMapRuntime || !context.fixedMirrors || !context.lightPuzzle ||
		!context.hazardLights) {
		return;
	}

	// 発射装置もこの一覧に含まれます。壁などのJSON配置物と同じ通常深度で描きます。
	SceneRenderPipeline::DrawObjects(context.object3dCommon, *context.sceneObjects);
	context.stageMapRuntime->Draw();
	for (const std::unique_ptr<FixedMirror>& fixedMirror : *context.fixedMirrors) {
		if (!fixedMirror) {
			continue;
		}
		fixedMirror->DrawSurface(*context.activeCamera);
		// Mirror用Pipelineの後、通常Object用Pipelineを次の描画へ戻します。
		context.object3dCommon->SetCommonDrawSetting();
	}
	if (context.mirrorFloor) {
		context.mirrorFloor->DrawSurface(*context.activeCamera);
		context.object3dCommon->SetCommonDrawSetting();
	}
	if (context.carryableMirror) {
		// 反射Cameraで使ったWVPが残らないよう、Game View用Cameraを描画直前に必ず設定し直します。
		// Playerと同じ通常Object3d経路で描くため、Mirror専用Pipelineへ依存しません。
		context.carryableMirror->GetObject().UpdateCameraForDraw(context.activeCamera);
		context.object3dCommon->SetCommonDrawSetting();
		context.carryableMirror->GetObject().Draw();
	}
	if (context.player) {
		context.player->GetObject().Draw();
	}
	// 発射装置はsceneObjectsの通常描画だけにし、壁越しのシルエットを重ねません。
	// 後続Objectが通常Pipelineを前提にできるよう、描画設定を戻します。
	context.object3dCommon->SetCommonDrawSetting();
	context.lightPuzzle->Draw(*context.activeCamera);
	context.hazardLights->Draw(*context.activeCamera);
}
