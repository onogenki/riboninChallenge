#include "DebugSceneContent.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "ModelManager.h"
#include "Object3dFactory.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include <algorithm>

// 棚の情報を使ってObject3dを生成し、必要なAnimationだけを設定します。
std::unique_ptr<Object3d> DebugSceneContent::CreateObject(
	const Context& context,
	const std::string& fileName,
	bool playAnimation)
{
	if (!context.modelLibrary) {
		return nullptr;
	}
	const auto modelIt = std::find_if(
		context.modelLibrary->begin(),
		context.modelLibrary->end(),
		[&](const SceneEditor::ShelfEntry& entry)
		{
			return entry.fileName == fileName;
		});
	if (modelIt == context.modelLibrary->end() || !modelIt->canLoad) {
		return nullptr;
	}

	auto object = Object3dFactory::Create(context.object3dCommon, fileName, playAnimation);
	if (!object) {
		return nullptr;
	}

	if (object->IsSkeletal() && modelIt->hasAnimation && playAnimation) {
		Object3dFactory::LoadAndPlayAnimation(*object, fileName);
	}

	return object;
}

// Camera前方へモデルを追加します。
bool DebugSceneContent::AddModel(const Context& context, const std::string& fileName)
{
	Vector3 spawnPosition{ 0.0f, 0.0f, 4.0f };
	if (context.camera) {
		spawnPosition = context.camera->GetTranslate();
		spawnPosition.z += 6.0f;
	}
	return AddModel(context, fileName, spawnPosition);
}

// 指定した3D座標へモデルを追加し、ECS登録とInspector選択をSceneへ通知します。
bool DebugSceneContent::AddModel(
	const Context& context,
	const std::string& fileName,
	const Vector3& spawnPosition)
{
	if (!context.normalObjects || !context.animationObjects) {
		return false;
	}
	auto object = CreateObject(context, fileName, true);
	if (!object) {
		return false;
	}

	object->SetTranslate(spawnPosition);
	const bool isAnimated = object->IsSkeletal();
	Object3d* addedObject = object.get();
	if (isAnimated) {
		context.animationObjects->push_back(std::move(object));
		if (context.selectObject) {
			context.selectObject(true, context.animationObjects->size() - 1);
		}
	} else {
		context.normalObjects->push_back(std::move(object));
		if (context.selectObject) {
			context.selectObject(false, context.normalObjects->size() - 1);
		}
	}
	if (context.registerModel) {
		context.registerModel(addedObject, fileName, isAnimated);
	}
	return true;
}

// 画面中央へTexture Spriteを追加します。
bool DebugSceneContent::AddTexture(const Context& context, const std::string& textureFilePath)
{
	if (!context.directXCommon) {
		return false;
	}
	const Vector2 position{
		static_cast<float>(context.directXCommon->GetClientWidth()) * 0.5f,
		static_cast<float>(context.directXCommon->GetClientHeight()) * 0.5f,
	};
	return AddTexture(context, textureFilePath, position);
}

// 指定した2D座標へTexture Spriteを追加し、ECS登録とInspector選択をSceneへ通知します。
bool DebugSceneContent::AddTexture(
	const Context& context,
	const std::string& textureFilePath,
	const Vector2& position)
{
	if (textureFilePath.empty() || !context.spriteCommon || !context.sprites) {
		return false;
	}
	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(context.spriteCommon, textureFilePath);
	sprite->SetAnchorPoint({ 0.5f, 0.5f });
	const Vector2 originalSize = sprite->GetSize();
	constexpr float kMaximumPreviewSize = 220.0f;
	const float largestSide = (std::max)(originalSize.x, originalSize.y);
	if (largestSide > kMaximumPreviewSize && largestSide > 0.0f) {
		const float scale = kMaximumPreviewSize / largestSide;
		sprite->SetSize({ originalSize.x * scale, originalSize.y * scale });
	}
	sprite->SetPosition(position);
	Sprite* addedSprite = sprite.get();
	context.sprites->push_back(std::move(sprite));
	if (context.selectSprite) {
		context.selectSprite(context.sprites->size() - 1);
	}
	if (context.registerSprite) {
		context.registerSprite(addedSprite, textureFilePath);
	}
	return true;
}

// 初期配置以外のモデル・Spriteを消し、ECSと選択状態を同期します。
void DebugSceneContent::ClearAdded(const Context& context)
{
	if (!context.normalObjects || !context.animationObjects || !context.sprites) {
		return;
	}
	if (context.directXCommon) {
		context.directXCommon->WaitForGPU();
	}
	if (context.normalObjects->size() > context.baseNormalObjectCount) {
		context.normalObjects->resize(context.baseNormalObjectCount);
	}
	if (context.animationObjects->size() > context.baseAnimationObjectCount) {
		context.animationObjects->resize(context.baseAnimationObjectCount);
	}
	if (context.sprites->size() > context.baseSpriteCount) {
		context.sprites->resize(context.baseSpriteCount);
	}
	if (context.updateEcsWorld) {
		context.updateEcsWorld();
	}
	if (context.clearObjectSelection) {
		context.clearObjectSelection();
	}
	if (context.clearSpriteSelection) {
		context.clearSpriteSelection();
	}
}
