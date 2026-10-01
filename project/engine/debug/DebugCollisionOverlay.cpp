#include "DebugCollisionOverlay.h"

#include "Camera.h"
#include "Model.h"
#include "../ImGui/ImGuiManager.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
	struct CollisionDebugBox
	{
		// 画面へ表示するワールド座標のAABBです。
		MyMath::AABB aabb{};
		// trueなら、別のAABBと重なっています。
		bool overlaps = false;
	};

	// 位置・回転・拡大率を含む行列で、モデル頂点をワールド座標へ変換します。
	Vector3 TransformPoint(const Vector3& point, const Matrix4x4& matrix)
	{
		return {
			point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0],
			point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1],
			point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2],
		};
	}

	// 二つのAABBが重なっているかを、各軸の範囲で判定します。
	bool IsAabbOverlapping(const MyMath::AABB& lhs, const MyMath::AABB& rhs)
	{
		return lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x &&
			lhs.min.y <= rhs.max.y && lhs.max.y >= rhs.min.y &&
			lhs.min.z <= rhs.max.z && lhs.max.z >= rhs.min.z;
	}
}

// Object3dの頂点をワールド座標へ変換し、Debug表示用のAABBを作ります。
bool DebugCollisionOverlay::BuildWorldAabb(const Object3d& object, MyMath::AABB& outAabb)
{
	Model* model = object.GetModel();
	if (!model || model->GetModelData().vertices.empty()) {
		return false;
	}

	const Transform& transform = object.GetTransform();
	const Matrix4x4 worldMatrix = MyMath::MakeAffineMatrix(
		transform.scale,
		transform.rotate,
		transform.translate);
	Vector3 minPoint{
		(std::numeric_limits<float>::max)(),
		(std::numeric_limits<float>::max)(),
		(std::numeric_limits<float>::max)(),
	};
	Vector3 maxPoint{
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
		std::numeric_limits<float>::lowest(),
	};

	for (const Model::VertexData& vertex : model->GetModelData().vertices) {
		const Vector3 localPosition{ vertex.position.x, vertex.position.y, vertex.position.z };
		const Vector3 worldPosition = TransformPoint(localPosition, worldMatrix);
		minPoint.x = (std::min)(minPoint.x, worldPosition.x);
		minPoint.y = (std::min)(minPoint.y, worldPosition.y);
		minPoint.z = (std::min)(minPoint.z, worldPosition.z);
		maxPoint.x = (std::max)(maxPoint.x, worldPosition.x);
		maxPoint.y = (std::max)(maxPoint.y, worldPosition.y);
		maxPoint.z = (std::max)(maxPoint.z, worldPosition.z);
	}

	outAabb.min = minPoint;
	outAabb.max = maxPoint;
	return true;
}

// Game ViewへColliderの線を描画します。重なりは赤、通常は青で表示します。
void DebugCollisionOverlay::Draw(const Context& context)
{
#ifdef USE_IMGUI
	if (!context.normalObjects || !context.animationObjects || !context.world || !context.camera) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}

	std::vector<CollisionDebugBox> boxes;
	auto appendObject = [&](const Object3d* object)
	{
		if (!object) {
			return;
		}
		const Ecs::Entity entity = context.world->FindEntity(object);
		if (entity != Ecs::kInvalidEntity && context.world->HasBoxCollider(entity)) {
			return;
		}
		CollisionDebugBox box{};
		if (BuildWorldAabb(*object, box.aabb)) {
			boxes.push_back(box);
		}
	};

	if (context.isPreviewMode && context.previewObject) {
		appendObject(context.previewObject);
	} else {
		for (const auto& object : *context.normalObjects) {
			appendObject(object.get());
		}
		for (const auto& object : *context.animationObjects) {
			appendObject(object.get());
		}
		for (Ecs::Entity entity : context.world->GetEntities()) {
			const Ecs::BoxColliderComponent* collider = context.world->GetBoxCollider(entity);
			if (collider && collider->enabled) {
				boxes.push_back({ collider->worldBounds, collider->isColliding });
			}
		}
	}

	for (size_t i = 0; i < boxes.size(); ++i) {
		for (size_t j = i + 1; j < boxes.size(); ++j) {
			if (IsAabbOverlapping(boxes[i].aabb, boxes[j].aabb)) {
				boxes[i].overlaps = true;
				boxes[j].overlaps = true;
			}
		}
	}

	const Matrix4x4& viewProjectionMatrix = context.camera->GetViewProjectionMatrix();
	const ImVec2 imageMin(rectX, rectY);
	const ImVec2 imageMax(rectX + rectWidth, rectY + rectHeight);
	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	drawList->PushClipRect(imageMin, imageMax, true);
	const auto projectToGameView = [&](const Vector3& position, ImVec2& screenPosition)
	{
		const float x = position.x * viewProjectionMatrix.m[0][0] + position.y * viewProjectionMatrix.m[1][0] + position.z * viewProjectionMatrix.m[2][0] + viewProjectionMatrix.m[3][0];
		const float y = position.x * viewProjectionMatrix.m[0][1] + position.y * viewProjectionMatrix.m[1][1] + position.z * viewProjectionMatrix.m[2][1] + viewProjectionMatrix.m[3][1];
		const float w = position.x * viewProjectionMatrix.m[0][3] + position.y * viewProjectionMatrix.m[1][3] + position.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3];
		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || w <= 0.0f) {
			return false;
		}
		screenPosition = ImVec2(
			imageMin.x + (x / w + 1.0f) * 0.5f * rectWidth,
			imageMin.y + (1.0f - y / w) * 0.5f * rectHeight);
		return std::isfinite(screenPosition.x) && std::isfinite(screenPosition.y);
	};

	constexpr std::array<std::pair<int, int>, 12> edges{ {
		{0, 1}, {1, 3}, {3, 2}, {2, 0},
		{4, 5}, {5, 7}, {7, 6}, {6, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7},
	} };
	for (const CollisionDebugBox& box : boxes) {
		const std::array<Vector3, 8> corners{ {
			{ box.aabb.min.x, box.aabb.min.y, box.aabb.min.z },
			{ box.aabb.max.x, box.aabb.min.y, box.aabb.min.z },
			{ box.aabb.min.x, box.aabb.max.y, box.aabb.min.z },
			{ box.aabb.max.x, box.aabb.max.y, box.aabb.min.z },
			{ box.aabb.min.x, box.aabb.min.y, box.aabb.max.z },
			{ box.aabb.max.x, box.aabb.min.y, box.aabb.max.z },
			{ box.aabb.min.x, box.aabb.max.y, box.aabb.max.z },
			{ box.aabb.max.x, box.aabb.max.y, box.aabb.max.z },
		} };
		std::array<ImVec2, 8> projected{};
		std::array<bool, 8> visible{};
		for (size_t i = 0; i < corners.size(); ++i) {
			visible[i] = projectToGameView(corners[i], projected[i]);
		}
		const ImU32 color = box.overlaps ? IM_COL32(255, 60, 60, 255) : IM_COL32(70, 160, 255, 255);
		for (const auto& edge : edges) {
			if (visible[edge.first] && visible[edge.second]) {
				drawList->AddLine(projected[edge.first], projected[edge.second], color, 2.0f);
			}
		}
	}
	drawList->PopClipRect();
#else
	(void)context;
#endif
}
