#include "ImGuiManager.h"

#include "Camera.h"
#include "Model.h"
#if __has_include("Laser.h")
#include "Laser.h"
#define CG2_HAS_LASER_DEBUG 1
#else
#define CG2_HAS_LASER_DEBUG 0
#endif
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "externals/imgui/imgui_internal.h"

namespace {

	// 3D座標をGame View内の画面座標へ変換し、Cameraの後ろ側ならfalseを返します。
	bool ProjectGameViewPoint(
		const Matrix4x4& viewProjectionMatrix,
		const ImVec2& imageMin,
		float imageWidth,
		float imageHeight,
		const Vector3& position,
		ImVec2& screenPosition)
	{
		const float x = position.x * viewProjectionMatrix.m[0][0] + position.y * viewProjectionMatrix.m[1][0] + position.z * viewProjectionMatrix.m[2][0] + viewProjectionMatrix.m[3][0];
		const float y = position.x * viewProjectionMatrix.m[0][1] + position.y * viewProjectionMatrix.m[1][1] + position.z * viewProjectionMatrix.m[2][1] + viewProjectionMatrix.m[3][1];
		const float w = position.x * viewProjectionMatrix.m[0][3] + position.y * viewProjectionMatrix.m[1][3] + position.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3];
		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 0.0f) {
			return false;
		}

		screenPosition = ImVec2(
			imageMin.x + (x / w + 1.0f) * 0.5f * imageWidth,
			imageMin.y + (1.0f - y / w) * 0.5f * imageHeight);
		return std::isfinite(screenPosition.x) && std::isfinite(screenPosition.y);
	}
}
#endif

// OBBをGame Viewへワイヤー表示し、衝突中は赤、非衝突時は青で描画します。
void ImGuiManager::DrawObbCollisionDebug(
	const MyMath::OBB& obb,
	const MyMath::Sphere& sphere,
	const Camera* camera,
	bool isColliding)
{
#ifdef USE_IMGUI
	(void)sphere;
	if (!camera) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}

	const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
	const ImVec2 imageMin(rectX, rectY);
	const ImVec2 imageMax(rectX + rectWidth, rectY + rectHeight);
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->PushClipRect(imageMin, imageMax, true);

	const auto makeCorner = [&](float signX, float signY, float signZ) {
		return Vector3{
			obb.center.x + obb.orientations[0].x * obb.size.x * signX + obb.orientations[1].x * obb.size.y * signY + obb.orientations[2].x * obb.size.z * signZ,
			obb.center.y + obb.orientations[0].y * obb.size.x * signX + obb.orientations[1].y * obb.size.y * signY + obb.orientations[2].y * obb.size.z * signZ,
			obb.center.z + obb.orientations[0].z * obb.size.x * signX + obb.orientations[1].z * obb.size.y * signY + obb.orientations[2].z * obb.size.z * signZ,
		};
	};

	const Vector3 corners[8]{
		makeCorner(-1.0f, -1.0f, -1.0f), makeCorner(1.0f, -1.0f, -1.0f),
		makeCorner(-1.0f, 1.0f, -1.0f), makeCorner(1.0f, 1.0f, -1.0f),
		makeCorner(-1.0f, -1.0f, 1.0f), makeCorner(1.0f, -1.0f, 1.0f),
		makeCorner(-1.0f, 1.0f, 1.0f), makeCorner(1.0f, 1.0f, 1.0f),
	};
	const int edges[12][2]{
		{0, 1}, {1, 3}, {3, 2}, {2, 0},
		{4, 5}, {5, 7}, {7, 6}, {6, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7},
	};

	ImVec2 projected[8]{};
	bool visible[8]{};
	for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
		visible[cornerIndex] = ProjectGameViewPoint(
			viewProjectionMatrix,
			imageMin,
			rectWidth,
			rectHeight,
			corners[cornerIndex],
			projected[cornerIndex]);
	}

	const ImU32 color = isColliding ? IM_COL32(255, 60, 60, 255) : IM_COL32(70, 160, 255, 255);
	for (const auto& edge : edges) {
		if (visible[edge[0]] && visible[edge[1]]) {
			drawList->AddLine(projected[edge[0]], projected[edge[1]], color, 2.0f);
		}
	}

	drawList->PopClipRect();
#else
	(void)obb;
	(void)sphere;
	(void)camera;
	(void)isColliding;
#endif
}

// 制御点をGame Viewへ線と番号で重ねて表示します。
void ImGuiManager::DrawControlPointPathDebug(
	const Vector3& basePosition,
	const std::vector<Vector3>& controlPoints,
	const Camera* camera)
{
#ifdef USE_IMGUI
	if (!camera || controlPoints.empty()) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}

	const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
	const ImVec2 imageMin(rectX, rectY);
	const ImVec2 imageMax(rectX + rectWidth, rectY + rectHeight);
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->PushClipRect(imageMin, imageMax, true);

	std::vector<ImVec2> projected(controlPoints.size());
	std::vector<bool> visible(controlPoints.size(), false);
	for (size_t index = 0; index < controlPoints.size(); ++index) {
		const Vector3 worldPoint{
			basePosition.x + controlPoints[index].x,
			basePosition.y + controlPoints[index].y,
			basePosition.z + controlPoints[index].z,
		};
		visible[index] = ProjectGameViewPoint(
			viewProjectionMatrix,
			imageMin,
			rectWidth,
			rectHeight,
			worldPoint,
			projected[index]);
	}

	const ImU32 lineColor = IM_COL32(80, 255, 140, 255);
	const ImU32 pointColor = IM_COL32(255, 220, 70, 255);
	for (size_t index = 1; index < projected.size(); ++index) {
		if (visible[index - 1] && visible[index]) {
			drawList->AddLine(projected[index - 1], projected[index], lineColor, 2.0f);
		}
	}
	for (size_t index = 0; index < projected.size(); ++index) {
		if (!visible[index]) {
			continue;
		}
		drawList->AddCircleFilled(projected[index], 5.0f, pointColor);
		const std::string label = std::to_string(index);
		drawList->AddText(
			ImVec2(projected[index].x + 7.0f, projected[index].y - 7.0f),
			pointColor,
			label.c_str());
	}

	drawList->PopClipRect();
#else
	(void)basePosition;
	(void)controlPoints;
	(void)camera;
#endif
}

// レーザー経路をGame Viewへ赤い線として重ねて表示します。
#if CG2_HAS_LASER_DEBUG
void ImGuiManager::DrawLaserDebug(
	const std::vector<LaserSegment>& segments,
	const Camera* camera)
{
#ifdef USE_IMGUI
	if (!camera || segments.empty()) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}

	const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
	const ImVec2 imageMin(rectX, rectY);
	const ImVec2 imageMax(rectX + rectWidth, rectY + rectHeight);
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->PushClipRect(imageMin, imageMax, true);

	for (const LaserSegment& segment : segments) {
		ImVec2 start{};
		ImVec2 end{};
		if (ProjectGameViewPoint(
			viewProjectionMatrix,
			imageMin,
			rectWidth,
			rectHeight,
			segment.start,
			start) &&
			ProjectGameViewPoint(
				viewProjectionMatrix,
				imageMin,
				rectWidth,
				rectHeight,
				segment.end,
				end)) {
			drawList->AddLine(start, end, IM_COL32(255, 40, 40, 255), 4.0f);
			drawList->AddCircleFilled(end, 4.0f, IM_COL32(255, 230, 120, 255));
		}
	}

	drawList->PopClipRect();
#else
	(void)segments;
	(void)camera;
#endif
}

#endif

// Playerの球Colliderを表示する。物体接触中は青、Laser接触中は優先して黄色にする。
void ImGuiManager::DrawPlayerCollisionDebug(
	const MyMath::Sphere& sphere,
	const Camera* camera,
	bool isObjectColliding,
	bool isLaserHit)
{
#ifdef USE_IMGUI
	if (!camera) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}

	const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
	const ImVec2 imageMin(rectX, rectY);
	const ImVec2 imageMax(rectX + rectWidth, rectY + rectHeight);
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->PushClipRect(imageMin, imageMax, true);

	// Laser判定を最優先し、物体接触の青色より危険を示す黄色を表示する。
	const ImU32 color = isLaserHit
		? IM_COL32(255, 225, 40, 255)
		: (isObjectColliding
			? IM_COL32(70, 160, 255, 255)
			: IM_COL32(235, 235, 235, 255));
	constexpr int kCircleDivision = 32;
	const auto drawCircle = [&](int plane) {
		for (int index = 0; index < kCircleDivision; ++index) {
			const float firstAngle =
				2.0f * std::numbers::pi_v<float> * static_cast<float>(index) /
				static_cast<float>(kCircleDivision);
			const float secondAngle =
				2.0f * std::numbers::pi_v<float> * static_cast<float>(index + 1) /
				static_cast<float>(kCircleDivision);
			const auto makePoint = [&](float angle) {
				Vector3 point = sphere.center;
				const float first = std::cos(angle) * sphere.radius;
				const float second = std::sin(angle) * sphere.radius;
				if (plane == 0) {
					point.x += first;
					point.y += second;
				} else if (plane == 1) {
					point.x += first;
					point.z += second;
				} else {
					point.y += first;
					point.z += second;
				}
				return point;
			};
			ImVec2 firstScreen{};
			ImVec2 secondScreen{};
			if (ProjectGameViewPoint(
				viewProjectionMatrix,
				imageMin,
				rectWidth,
				rectHeight,
				makePoint(firstAngle),
				firstScreen) &&
				ProjectGameViewPoint(
					viewProjectionMatrix,
					imageMin,
					rectWidth,
					rectHeight,
					makePoint(secondAngle),
					secondScreen)) {
				drawList->AddLine(firstScreen, secondScreen, color, 3.0f);
			}
		}
	};
	drawCircle(0);
	drawCircle(1);
	drawCircle(2);
	drawList->PopClipRect();
#else
	(void)sphere;
	(void)camera;
	(void)isObjectColliding;
	(void)isLaserHit;
#endif
}

// スケルトンの親子関係をGame Viewへ線と点で重ねて表示します。
bool ImGuiManager::IsSkeletonDebugDrawEnabled() const
{
#ifdef USE_IMGUI
	return showSkeletonDebugDraw_;
#else
	return false;
#endif
}

// アニメーション済みのJoint座標を、Game View上の線と点へ変換して描画します。
void ImGuiManager::SkeletonDebugDraw(
	const Model::Skeleton& skeleton,
	const Matrix4x4& worldMatrix,
	const Matrix4x4& viewProjectionMatrix)
{
#ifdef USE_IMGUI
	if (!showSkeletonDebugDraw_) {
		return;
	}

	ImDrawList* drawList = gameViewDrawList_;
	const ImVec2 imageMin = gameViewImageMin_;
	const ImVec2 imageSize = gameViewImageSize_;
	if (!drawList || imageSize.x <= 0.0f || imageSize.y <= 0.0f) {
		return;
	}

	const Matrix4x4 worldViewProjection =
		MyMath::Multiply(worldMatrix, viewProjectionMatrix);
	const auto projectToGameView = [&](const Vector3& position, ImVec2& screenPosition) {
		const float x = position.x * worldViewProjection.m[0][0] + position.y * worldViewProjection.m[1][0] + position.z * worldViewProjection.m[2][0] + worldViewProjection.m[3][0];
		const float y = position.x * worldViewProjection.m[0][1] + position.y * worldViewProjection.m[1][1] + position.z * worldViewProjection.m[2][1] + worldViewProjection.m[3][1];
		const float w = position.x * worldViewProjection.m[0][3] + position.y * worldViewProjection.m[1][3] + position.z * worldViewProjection.m[2][3] + worldViewProjection.m[3][3];
		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 0.0f) {
			return false;
		}
		screenPosition = ImVec2(
			imageMin.x + (x / w + 1.0f) * 0.5f * imageSize.x,
			imageMin.y + (1.0f - y / w) * 0.5f * imageSize.y);
		return std::isfinite(screenPosition.x) && std::isfinite(screenPosition.y);
	};

	drawList->PushClipRect(
		imageMin,
		ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y),
		true);
	for (const Model::Joint& joint : skeleton.joints) {
		if (!joint.parent.has_value()) {
			continue;
		}
		const size_t parentIndex = joint.parent.value();
		if (parentIndex >= skeleton.joints.size()) {
			continue;
		}

		const Model::Joint& parentJoint = skeleton.joints[parentIndex];
		const Vector3 jointPosition{
			joint.skeletonSpaceMatrix.m[3][0],
			joint.skeletonSpaceMatrix.m[3][1],
			joint.skeletonSpaceMatrix.m[3][2],
		};
		const Vector3 parentPosition{
			parentJoint.skeletonSpaceMatrix.m[3][0],
			parentJoint.skeletonSpaceMatrix.m[3][1],
			parentJoint.skeletonSpaceMatrix.m[3][2],
		};
		ImVec2 jointScreenPosition{};
		ImVec2 parentScreenPosition{};
		if (projectToGameView(jointPosition, jointScreenPosition) &&
			projectToGameView(parentPosition, parentScreenPosition)) {
			drawList->AddLine(
				parentScreenPosition,
				jointScreenPosition,
				IM_COL32(255, 255, 0, 255),
				2.0f);
		}
	}

	for (const Model::Joint& joint : skeleton.joints) {
		const Vector3 jointPosition{
			joint.skeletonSpaceMatrix.m[3][0],
			joint.skeletonSpaceMatrix.m[3][1],
			joint.skeletonSpaceMatrix.m[3][2],
		};
		ImVec2 jointScreenPosition{};
		if (projectToGameView(jointPosition, jointScreenPosition)) {
			drawList->AddCircleFilled(
				jointScreenPosition,
				5.0f,
				IM_COL32(255, 0, 0, 255));
		}
	}
	drawList->PopClipRect();
#endif
}
