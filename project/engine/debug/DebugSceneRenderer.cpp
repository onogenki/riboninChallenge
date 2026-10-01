#include "DebugSceneRenderer.h"

#include "DirectXCommon.h"
#include "GPUParticle.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "ParticleManager.h"
#include "SceneRenderPipeline.h"
#include "SkyBox.h"
#include "Sprite.h"
#include "SpriteCommon.h"

// 3D背景・Particle・Sprite・PostEffect・ImGuiの順でDebug Sceneを描画します。
void DebugSceneRenderer::Draw(const Context& context)
{
	if (!context.directXCommon || !context.object3dCommon || !context.spriteCommon ||
		!context.normalObjects || !context.animationObjects || !context.sprites) {
		return;
	}

	SceneRenderPipeline::Begin(context.directXCommon);

	if (context.previewObject) {
		context.object3dCommon->SetCommonDrawSetting();
		context.previewObject->Draw();
	} else if (context.previewSprite) {
		context.spriteCommon->SetCommonDrawSetting();
		context.previewSprite->Draw();
	} else {
		SceneRenderPipeline::DrawObjects(
			context.object3dCommon,
			*context.normalObjects,
			SceneRenderPipeline::ObjectDrawFilter::kNonSkeletalOnly);
		if (context.skyBox) {
			context.skyBox->Draw();
		}

		SceneRenderPipeline::DrawObjects(
			context.object3dCommon,
			*context.animationObjects,
			SceneRenderPipeline::ObjectDrawFilter::kSkeletalOnly);
		if (context.handWeapon) {
			// Object3d::Drawが通常モデル用の設定を選ぶため、Scene側で骨用設定を戻す必要はありません。
			context.handWeapon->Draw();
		}

		ParticleManager::GetInstance()->Draw();
		GPUParticle::GetInstance()->Draw(context.camera);

		SceneRenderPipeline::DrawSprites(context.spriteCommon, *context.sprites);
	}

	SceneRenderPipeline::End(context.directXCommon, context.camera);
}
