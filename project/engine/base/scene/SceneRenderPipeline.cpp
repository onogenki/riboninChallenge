#include "SceneRenderPipeline.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "MyMath.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "PostEffect.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "SrvManager.h"

// RenderTextureへSceneを描画する前に、共通のDirectXとSRV設定を行います。
void SceneRenderPipeline::Begin(DirectXCommon* directXCommon, const Vector4* clearColorOverride)
{
	if (!directXCommon) {
		return;
	}

	directXCommon->PreDraw(clearColorOverride);
	SrvManager::GetInstance()->PreDraw();
}

// Sceneごとの重複したDrawループをなくし、3D一覧の描画条件を一か所へそろえます。
void SceneRenderPipeline::DrawObjects(
	Object3dCommon* object3dCommon,
	const std::vector<std::unique_ptr<Object3d>>& objects,
	ObjectDrawFilter filter)
{
	if (!object3dCommon) {
		return;
	}

	object3dCommon->SetCommonDrawSetting();
	for (const std::unique_ptr<Object3d>& object : objects) {
		if (!object) {
			continue;
		}

		const bool isSkeletal = object->IsSkeletal();
		if ((filter == ObjectDrawFilter::kSkeletalOnly && !isSkeletal) ||
			(filter == ObjectDrawFilter::kNonSkeletalOnly && isSkeletal)) {
			continue;
		}
		object->Draw();
	}
}

// Sceneごとの重複したSprite描画ループをなくし、共通設定漏れを防ぎます。
void SceneRenderPipeline::DrawSprites(
	SpriteCommon* spriteCommon,
	const std::vector<std::unique_ptr<Sprite>>& sprites,
	size_t firstSpriteIndex)
{
	if (!spriteCommon) {
		return;
	}

	spriteCommon->SetCommonDrawSetting();
	// Titleの導入Spriteだけを除外できるよう、指定位置から描画します。
	for (size_t index = firstSpriteIndex; index < sprites.size(); ++index) {
		const std::unique_ptr<Sprite>& sprite = sprites[index];
		if (sprite) {
			sprite->Draw();
		}
	}
}

// Scene描画後のPostEffect・SwapChain・ImGui・Presentを共通の順番で実行します。
void SceneRenderPipeline::End(
	DirectXCommon* directXCommon,
	const Camera* activeCamera,
	bool drawImGui)
{
	if (!directXCommon) {
		return;
	}

	PostEffect* postEffect = PostEffect::GetInstance();
	const bool isPostEffectEnabled = postEffect->IsEnabled();
	if (isPostEffectEnabled) {
		if (postEffect->IsGaussianFilter()) {
			directXCommon->PreDrawForGaussianHorizontalTexture();
			postEffect->Draw(directXCommon->GetRenderTextureSrvIndex(), true);
			directXCommon->PreDrawForGaussianVerticalTexture();
			postEffect->DrawGaussianVertical(directXCommon->GetGaussianBlurTextureSrvIndex());
		} else if (postEffect->IsDepthBasedOutline()) {
			if (activeCamera) {
				postEffect->SetProjectionInverse(MyMath::Inverse(activeCamera->GetProjectionMatrix()));
			}
			directXCommon->PreDrawForDepthBasedOutlineTexture();
			postEffect->Draw(directXCommon->GetRenderTextureSrvIndex(), true);
		} else {
			directXCommon->PreDrawForPostEffectTexture();
			postEffect->Draw(directXCommon->GetRenderTextureSrvIndex(), true);
		}
	}

	directXCommon->PreDrawForSwapChain(isPostEffectEnabled);
#ifndef USE_IMGUI
	// ImGuiを含まない構成では、RenderTextureのSceneを全画面三角形でSwapChainへコピーします。
	postEffect->Draw(
		isPostEffectEnabled
			? directXCommon->GetPostEffectTextureSrvIndex()
			: directXCommon->GetRenderTextureSrvIndex(),
		false);
#endif
	if (drawImGui) {
		ImGuiManager::GetInstance()->Draw(directXCommon);
	}
	directXCommon->PostDraw();
}
