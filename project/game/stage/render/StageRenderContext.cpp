#include "StageRenderContext.h"

#include "CarryableMirror.h"
#include "FixedMirror.h"
#include "Object3dRenderContext.h"
#include "Player.h"
#include "SkyBox.h"
#include "StageLighting.h"
#include "StageMapRuntime.h"

// Stageが決めたCamera・Lightを全Objectへ同じ順番で渡し、描画前の行列をそろえます。
void StageRenderContext::Prepare(const Context& context)
{
	Object3dRenderContext renderContext(
		context.activeCamera,
		context.lighting.GetDirectionalLight(),
		context.lighting.GetPointLight(),
		context.lighting.GetSpotLights());
	renderContext.UpdateObjects(context.sceneObjects);
	context.stageMapRuntime.UpdateRenderObjects(renderContext);
	for (const auto& fixedMirror : context.fixedMirrors) {
		if (fixedMirror) {
			renderContext.UpdateObject(fixedMirror->GetObject());
		}
	}
	if (context.mirrorFloor) {
		renderContext.UpdateObject(context.mirrorFloor->GetObject());
	}
	if (context.carryableMirror) {
		renderContext.UpdateObject(context.carryableMirror->GetObject());
	}
	if (context.player) {
		renderContext.UpdateObject(context.player->GetObject());
	}
	// Cameraの更新後にSkyBoxの行列を作り、仮Titleと本編で同じ向きを描きます。
	context.skyBox.Update();
}
