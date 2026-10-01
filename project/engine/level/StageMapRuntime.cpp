#include "StageMapRuntime.h"

#include "Object3dRenderContext.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace
{
	// Catmull-Rom曲線の一成分を計算します。
	float CatmullRomValue(float p0, float p1, float p2, float p3, float t)
	{
		const float t2 = t * t;
		const float t3 = t2 * t;
		return 0.5f * (
			2.0f * p1 +
			(-p0 + p2) * t +
			(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
			(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
	}

	// 制御点列と進行度から、現在の移動量を計算します。
	Vector3 EvaluateControlPointPath(
		const std::vector<Vector3>& controlPoints,
		float progress,
		bool loop)
	{
		if (controlPoints.empty()) {
			return {};
		}
		if (controlPoints.size() == 1) {
			return controlPoints.front();
		}

		const int pointCount = static_cast<int>(controlPoints.size());
		const int segmentCount = loop ? pointCount : pointCount - 1;
		float pathPosition = progress;
		if (loop) {
			pathPosition = std::fmod(pathPosition, static_cast<float>(segmentCount));
			if (pathPosition < 0.0f) {
				pathPosition += static_cast<float>(segmentCount);
			}
		} else {
			pathPosition = std::clamp(
				pathPosition,
				0.0f,
				static_cast<float>(segmentCount));
		}

		int segmentIndex = static_cast<int>(std::floor(pathPosition));
		float segmentT = pathPosition - static_cast<float>(segmentIndex);
		if (!loop && segmentIndex >= segmentCount) {
			segmentIndex = segmentCount - 1;
			segmentT = 1.0f;
		}

		auto getPoint = [&](int index) -> const Vector3&
		{
			if (loop) {
				index %= pointCount;
				if (index < 0) {
					index += pointCount;
				}
			} else {
				index = std::clamp(index, 0, pointCount - 1);
			}
			return controlPoints[index];
		};

		const Vector3& p0 = getPoint(segmentIndex - 1);
		const Vector3& p1 = getPoint(segmentIndex);
		const Vector3& p2 = getPoint(segmentIndex + 1);
		const Vector3& p3 = getPoint(segmentIndex + 2);
		return {
			CatmullRomValue(p0.x, p1.x, p2.x, p3.x, segmentT),
			CatmullRomValue(p0.y, p1.y, p2.y, p3.y, segmentT),
			CatmullRomValue(p0.z, p1.z, p2.z, p3.z, segmentT),
		};
	}

	// ObjectDataのCollider設定をRuntimeObjectへコピーします。
	void ApplyCollider(
		StageMapRuntime::RuntimeObject& runtimeObject,
		const LevelLoader::ObjectData& objectData)
	{
		runtimeObject.hasBoxCollider =
			objectData.hasCollider && objectData.collider.type == "BOX";
		if (!runtimeObject.hasBoxCollider) {
			return;
		}

		const Vector3 localHalfSize{
			std::abs(objectData.collider.size.x) * 0.5f,
			std::abs(objectData.collider.size.y) * 0.5f,
			std::abs(objectData.collider.size.z) * 0.5f,
		};
		runtimeObject.collider.SetLocalShape(objectData.collider.center, localHalfSize);
	}
}

bool StageMapRuntime::Rebuild(
	const std::vector<const LevelLoader::ObjectData*>& objectDataList,
	const CreateObjectFunction& createObject)
{
	std::vector<RuntimeObject> rebuiltObjects;
	rebuiltObjects.reserve(objectDataList.size());
	for (const LevelLoader::ObjectData* objectData : objectDataList) {
		if (!objectData) {
			return false;
		}

		RuntimeObject runtimeObject{};
		runtimeObject.sourceName = objectData->name;
		runtimeObject.tag = objectData->tag;
		// Factoryを使うCreateObjectFunctionがモデル読込とObject3d初期化をまとめて行います。
		// Runtimeは生成結果だけを見るため、ModelManagerへ直接依存しません。
		runtimeObject.visual = createObject(objectData->fileName);
		if (!runtimeObject.visual) {
			return false;
		}
		runtimeObject.visual->SetTranslate(objectData->translation);
		runtimeObject.visual->SetRotate(objectData->rotation);
		runtimeObject.visual->SetScale(objectData->scaling);
		runtimeObject.pathBasePosition = objectData->translation;
		runtimeObject.controlPoints = objectData->controlPoints;
		runtimeObject.pathSpeed = objectData->pathSpeed;
		runtimeObject.pathLoop = objectData->pathLoop;
		ApplyCollider(runtimeObject, *objectData);
		rebuiltObjects.push_back(std::move(runtimeObject));
	}

	objects_ = std::move(rebuiltObjects);
	SyncColliders();
	return true;
}

void StageMapRuntime::ApplyEdits(
	const std::vector<const LevelLoader::ObjectData*>& objectDataList)
{
	for (RuntimeObject& runtimeObject : objects_) {
		const auto found = std::find_if(
			objectDataList.begin(),
			objectDataList.end(),
			[&](const LevelLoader::ObjectData* objectData)
			{
				return objectData && objectData->name == runtimeObject.sourceName;
			});
		if (found == objectDataList.end() || !runtimeObject.visual) {
			continue;
		}

		const LevelLoader::ObjectData& objectData = **found;
		runtimeObject.tag = objectData.tag;
		runtimeObject.visual->SetTranslate(objectData.translation);
		runtimeObject.visual->SetRotate(objectData.rotation);
		runtimeObject.visual->SetScale(objectData.scaling);
		runtimeObject.pathBasePosition = objectData.translation;
		runtimeObject.controlPoints = objectData.controlPoints;
		runtimeObject.pathSpeed = objectData.pathSpeed;
		runtimeObject.pathLoop = objectData.pathLoop;
		ApplyCollider(runtimeObject, objectData);
	}
	SyncColliders();
}

void StageMapRuntime::UpdatePaths(float deltaTime)
{
	for (RuntimeObject& runtimeObject : objects_) {
		if (!runtimeObject.visual || runtimeObject.controlPoints.size() < 2) {
			continue;
		}

		runtimeObject.pathProgress +=
			(std::max)(runtimeObject.pathSpeed, 0.0f) * deltaTime;
		const int segmentCount = runtimeObject.pathLoop
			? static_cast<int>(runtimeObject.controlPoints.size())
			: static_cast<int>(runtimeObject.controlPoints.size()) - 1;
		if (!runtimeObject.pathLoop) {
			runtimeObject.pathProgress = (std::min)(
				runtimeObject.pathProgress,
				static_cast<float>(segmentCount));
		}

		const Vector3 pathOffset = EvaluateControlPointPath(
			runtimeObject.controlPoints,
			runtimeObject.pathProgress,
			runtimeObject.pathLoop);
		runtimeObject.visual->SetTranslate({
			runtimeObject.pathBasePosition.x + pathOffset.x,
			runtimeObject.pathBasePosition.y + pathOffset.y,
			runtimeObject.pathBasePosition.z + pathOffset.z,
		});
	}
	SyncColliders();
}

// JSON・CSVから生成して所有するモデルだけへ、共通の描画準備をまとめて適用します。
void StageMapRuntime::UpdateRenderObjects(const Object3dRenderContext& renderContext)
{
	for (RuntimeObject& runtimeObject : objects_) {
		if (runtimeObject.visual) {
			renderContext.UpdateObject(*runtimeObject.visual);
		}
	}
}

// 一枚のMirror反射Cameraまたは通常Cameraの行列を、所有する配置物全てへ反映します。
void StageMapRuntime::UpdateCameraForDraw(Camera* camera)
{
	for (RuntimeObject& runtimeObject : objects_) {
		if (runtimeObject.visual) {
			runtimeObject.visual->UpdateCameraForDraw(camera);
		}
	}
}

// Sceneが決めた描画順の中で、Runtimeが所有する通常配置物だけを描画します。
void StageMapRuntime::Draw() const
{
	for (const RuntimeObject& runtimeObject : objects_) {
		if (runtimeObject.visual) {
			runtimeObject.visual->Draw();
		}
	}
}

void StageMapRuntime::SyncColliders()
{
	for (RuntimeObject& runtimeObject : objects_) {
		if (!runtimeObject.hasBoxCollider || !runtimeObject.visual) {
			continue;
		}
		runtimeObject.collider.SyncTransform(runtimeObject.visual->GetTransform());
	}
}

void StageMapRuntime::Clear()
{
	objects_.clear();
}
