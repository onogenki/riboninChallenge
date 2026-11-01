#include "StageEditHistory.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace
{
	bool EqualVector3(const Vector3& a, const Vector3& b)
	{
		return a.x == b.x && a.y == b.y && a.z == b.z;
	}

	bool EqualObject(const LevelLoader::ObjectData& a, const LevelLoader::ObjectData& b);

	// PlayerStartは履歴対象から外し、それ以外の配置と順序を比較します。
	bool EqualObjects(
		const std::vector<LevelLoader::ObjectData>& a,
		const std::vector<LevelLoader::ObjectData>& b)
	{
		size_t aIndex = 0;
		size_t bIndex = 0;
		while (true) {
			while (aIndex < a.size() && a[aIndex].tag == "PlayerStart") {
				++aIndex;
			}
			while (bIndex < b.size() && b[bIndex].tag == "PlayerStart") {
				++bIndex;
			}
			if (aIndex == a.size() || bIndex == b.size()) {
				return aIndex == a.size() && bIndex == b.size();
			}
			if (!EqualObject(a[aIndex], b[bIndex])) {
				return false;
			}
			++aIndex;
			++bIndex;
		}
	}

	// JSONへ保存する配置・判定・Camera・制御点の値を、編集差分として比較します。
	bool EqualObject(const LevelLoader::ObjectData& a, const LevelLoader::ObjectData& b)
	{
		if (a.type != b.type || a.name != b.name || a.tag != b.tag ||
			a.objectType != b.objectType || a.fileName != b.fileName ||
			!EqualVector3(a.translation, b.translation) ||
			!EqualVector3(a.rotation, b.rotation) ||
			!EqualVector3(a.scaling, b.scaling) ||
			a.eventId != b.eventId || a.eventCameraName != b.eventCameraName ||
			a.hasCameraFocus != b.hasCameraFocus ||
			!EqualVector3(a.cameraFocus, b.cameraFocus) ||
			a.hasCameraArea != b.hasCameraArea ||
			a.cameraArea.distance != b.cameraArea.distance ||
			a.cameraArea.pitch != b.cameraArea.pitch ||
			a.cameraArea.fovY != b.cameraArea.fovY ||
			a.pathSpeed != b.pathSpeed || a.pathLoop != b.pathLoop ||
			a.hasCollider != b.hasCollider || a.collider.type != b.collider.type ||
			!EqualVector3(a.collider.center, b.collider.center) ||
			!EqualVector3(a.collider.size, b.collider.size) ||
			a.controlPoints.size() != b.controlPoints.size()) {
			return false;
		}
		for (size_t index = 0; index < a.controlPoints.size(); ++index) {
			if (!EqualVector3(a.controlPoints[index], b.controlPoints[index])) {
				return false;
			}
		}
		return EqualObjects(a.children, b.children);
	}

	bool EqualSpotLight(
		const LevelLoader::SpotLightData& a,
		const LevelLoader::SpotLightData& b)
	{
		return a.name == b.name && EqualVector3(a.color, b.color) &&
			EqualVector3(a.position, b.position) && a.intensity == b.intensity &&
			EqualVector3(a.direction, b.direction) && a.distance == b.distance &&
			a.decay == b.decay && a.cosAngle == b.cosAngle &&
			a.cosFalloffStart == b.cosFalloffStart;
	}

	// Stage Lightingの数値もJSON編集に含め、通常Cameraの一時操作とは分けます。
	bool EqualLighting(
		const LevelLoader::LightingData& a,
		const LevelLoader::LightingData& b)
	{
		if (!EqualVector3(a.directionalColor, b.directionalColor) ||
			!EqualVector3(a.directionalDirection, b.directionalDirection) ||
			a.directionalIntensity != b.directionalIntensity ||
			!EqualVector3(a.ambientColor, b.ambientColor) ||
			a.ambientIntensity != b.ambientIntensity ||
			!EqualVector3(a.pointColor, b.pointColor) ||
			!EqualVector3(a.pointPosition, b.pointPosition) ||
			a.pointIntensity != b.pointIntensity || a.pointRadius != b.pointRadius ||
			a.pointDecay != b.pointDecay || a.spotLights.size() != b.spotLights.size()) {
			return false;
		}
		for (size_t index = 0; index < a.spotLights.size(); ++index) {
			if (!EqualSpotLight(a.spotLights[index], b.spotLights[index])) {
				return false;
			}
		}
		return true;
	}

	bool EqualUndoableData(
		const LevelLoader::LevelData& a,
		const LevelLoader::LevelData& b)
	{
		return a.hasLighting == b.hasLighting &&
			(!a.hasLighting || EqualLighting(a.lighting, b.lighting)) &&
			EqualObjects(a.objects, b.objects);
	}

	// 子Object内のPlayerStartも現在の値を残し、モデル編集のUndoへ巻き込みません。
	void PreservePlayerStarts(
		std::vector<LevelLoader::ObjectData>& target,
		const std::vector<LevelLoader::ObjectData>& current)
	{
		target.erase(
			std::remove_if(target.begin(), target.end(),
				[](const LevelLoader::ObjectData& objectData)
				{
					return objectData.tag == "PlayerStart";
				}),
			target.end());
		for (size_t index = 0; index < current.size(); ++index) {
			if (current[index].tag == "PlayerStart") {
				target.insert(
					target.begin() + (std::min)(index, target.size()),
					current[index]);
			}
		}
		for (LevelLoader::ObjectData& objectData : target) {
			const auto found = std::find_if(current.begin(), current.end(),
				[&](const LevelLoader::ObjectData& currentObject)
				{
					return currentObject.name == objectData.name &&
						currentObject.tag != "PlayerStart";
				});
			if (found != current.end()) {
				PreservePlayerStarts(objectData.children, found->children);
			}
		}
	}

	// PlayerStartの現在値と保存座標系は、Undo/Redo対象の配置を戻す時にも保持します。
	void PreserveExcludedData(
		LevelLoader::LevelData& target,
		const LevelLoader::LevelData& current)
	{
		target.coordinateSystem = current.coordinateSystem;
		PreservePlayerStarts(target.objects, current.objects);
	}
}

// 直前の変更前だけを保持し、ドラッグ途中では履歴を上書きせず最新値だけ観測します。
void StageEditHistory::Observe(
	const LevelLoader::LevelData& levelData,
	int selectedObjectIndex,
	bool isEditingContinuously)
{
	Snapshot current{ levelData, selectedObjectIndex };
	if (!observed_) {
		observed_ = std::move(current);
		return;
	}
	if (!EqualUndoableData(observed_->levelData, current.levelData)) {
		if (!isContinuousEdit_) {
			undo_ = *observed_;
			redo_.reset();
		}
		isContinuousEdit_ = isEditingContinuously;
	} else if (!isEditingContinuously) {
		isContinuousEdit_ = false;
	}
	observed_ = std::move(current);
}

// 一つ前の編集だけを反映し、失敗時はデータと履歴の両方を動かしません。
bool StageEditHistory::Undo(
	LevelLoader::LevelData& levelData,
	int& selectedObjectIndex,
	const std::function<bool()>& apply)
{
	return Restore(undo_, redo_, levelData, selectedObjectIndex, apply);
}

// Undo直前の編集へ一手順進め、再構築に失敗した時は現在のStageを保ちます。
bool StageEditHistory::Redo(
	LevelLoader::LevelData& levelData,
	int& selectedObjectIndex,
	const std::function<bool()>& apply)
{
	return Restore(redo_, undo_, levelData, selectedObjectIndex, apply);
}

// Playerと座標系を維持した候補を先に反映し、成功後だけ履歴の位置を変えます。
bool StageEditHistory::Restore(
	std::optional<Snapshot>& source,
	std::optional<Snapshot>& destination,
	LevelLoader::LevelData& levelData,
	int& selectedObjectIndex,
	const std::function<bool()>& apply)
{
	if (!source || !apply) {
		return false;
	}
	Snapshot previous{ levelData, selectedObjectIndex };
	LevelLoader::LevelData restored = source->levelData;
	PreserveExcludedData(restored, levelData);
	levelData = std::move(restored);
	selectedObjectIndex = source->selectedObjectIndex;
	if (!apply()) {
		levelData = std::move(previous.levelData);
		selectedObjectIndex = previous.selectedObjectIndex;
		apply();
		return false;
	}
	destination = std::move(previous);
	source.reset();
	observed_ = Snapshot{ levelData, selectedObjectIndex };
	isContinuousEdit_ = false;
	return true;
}

// StageやSceneをまたいで古い配置を復元しないよう、全ての観測と履歴を消します。
void StageEditHistory::Clear()
{
	observed_.reset();
	undo_.reset();
	redo_.reset();
	isContinuousEdit_ = false;
}
