#include "StageMirrorFactory.h"

#include "CarryableMirror.h"
#include "DirectXCommon.h"
#include "FixedMirror.h"
#include "SrvManager.h"
#include <cmath>
#include <utility>

namespace
{
	// JSONのBOXは全辺長、Colliderは半辺長なので、生成時と編集時で同じ変換を使います。
	bool ApplyBoxColliderData(FixedMirror& mirror, const LevelLoader::ObjectData& data)
	{
		if (!data.hasCollider || data.collider.type != "BOX") {
			return false;
		}
		mirror.SetColliderShape(
			data.collider.center,
			{
				std::abs(data.collider.size.x) * 0.5f,
				std::abs(data.collider.size.y) * 0.5f,
				std::abs(data.collider.size.z) * 0.5f,
			});
		return true;
	}
}

// 携帯鏡自身にModelとColliderの準備を任せ、生成できた時だけSceneへ渡します。
std::unique_ptr<CarryableMirror> StageMirrorFactory::CreateCarryableMirror(
	Object3dCommon* object3dCommon,
	const std::string& modelName,
	const Vector3& position,
	float width,
	float height)
{
	auto mirror = std::make_unique<CarryableMirror>();
	if (!mirror->Initialize(object3dCommon, modelName, position, width, height)) {
		return nullptr;
	}
	return mirror;
}

// 鏡床固有の向き・両面反射・Collider同期をまとめ、Sceneに設定漏れを作らないようにします。
std::unique_ptr<FixedMirror> StageMirrorFactory::CreateMirrorFloor(
	Object3dCommon* object3dCommon,
	const std::string& modelName,
	const Vector3& position,
	float width,
	float height)
{
	auto mirror = std::make_unique<FixedMirror>();
	if (!mirror->Initialize(
		object3dCommon,
		DirectXCommon::GetInstance(),
		SrvManager::GetInstance(),
		modelName,
		position,
		0.0f,
		width,
		height,
		256)) {
		return nullptr;
	}
	mirror->SetPitch(-1.57079633f);
	mirror->SyncVisualAndCollider();
	// 鏡床は上下どちらから来たLightも反射する特殊ギミックです。
	mirror->GetMirror().SetReflectBackface(true);
	return mirror;
}

// Sceneの本番一覧とは別の仮一覧へ、反射TextureとColliderを持つMirrorを順番に作ります。
bool StageMirrorFactory::CreateFixedMirrors(
	Object3dCommon* object3dCommon,
	const std::vector<const LevelLoader::ObjectData*>& mirrorDataList,
	std::vector<std::unique_ptr<FixedMirror>>& outFixedMirrors)
{
	outFixedMirrors.clear();
	outFixedMirrors.reserve(mirrorDataList.size());
	for (const LevelLoader::ObjectData* mirrorData : mirrorDataList) {
		if (!mirrorData) {
			return false;
		}

		auto fixedMirror = std::make_unique<FixedMirror>();
		if (!fixedMirror->Initialize(
			object3dCommon,
			DirectXCommon::GetInstance(),
			SrvManager::GetInstance(),
			mirrorData->fileName.empty() ? "debug/plane.obj" : mirrorData->fileName,
			mirrorData->translation,
			mirrorData->rotation.y,
			std::abs(mirrorData->scaling.x) * 2.0f,
			std::abs(mirrorData->scaling.y) * 2.0f,
			512)) {
			return false;
		}

		// BOX指定がなければ、FixedMirror::Initializeが設定した従来の判定形状を使います。
		ApplyBoxColliderData(*fixedMirror, *mirrorData);
		outFixedMirrors.push_back(std::move(fixedMirror));
	}
	return true;
}

// Editorで変更した配置だけを既存Mirrorへ反映し、反射Textureの再生成を避けます。
bool StageMirrorFactory::ApplyFixedMirrorEdits(
	const std::vector<const LevelLoader::ObjectData*>& mirrorDataList,
	std::vector<std::unique_ptr<FixedMirror>>& fixedMirrors)
{
	if (fixedMirrors.size() != mirrorDataList.size()) {
		return false;
	}

	for (size_t index = 0; index < mirrorDataList.size(); ++index) {
		const LevelLoader::ObjectData* mirrorData = mirrorDataList[index];
		FixedMirror* fixedMirror = fixedMirrors[index].get();
		if (!mirrorData || !fixedMirror) {
			return false;
		}

		fixedMirror->GetYawForEdit() = mirrorData->rotation.y;
		fixedMirror->GetMirror().SetCenter(mirrorData->translation);
		fixedMirror->GetMirror().SetSize(
			std::abs(mirrorData->scaling.x) * 2.0f,
			std::abs(mirrorData->scaling.y) * 2.0f);
		if (!ApplyBoxColliderData(*fixedMirror, *mirrorData)) {
			// BOX指定がない編集でも既存のローカル形状は残し、Transformだけを同期します。
			fixedMirror->SyncVisualAndCollider();
		}
	}
	return true;
}
