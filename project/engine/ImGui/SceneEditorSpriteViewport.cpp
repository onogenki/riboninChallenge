#include "SceneEditor.h"

#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "SceneEditorViewportMath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#endif

// Game View上でSpriteを選択し、移動・回転・大きさを編集します。
void SceneEditor::DrawSpriteViewportEditor(SpriteViewportState& state, const SpriteViewportOptions& options)
{
#ifdef USE_IMGUI
	if (!ImGuiManager::GetInstance()->IsEditViewActive()) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}
	const float clientWidth = static_cast<float>(DirectXCommon::GetInstance()->GetClientWidth());
	const float clientHeight = static_cast<float>(DirectXCommon::GetInstance()->GetClientHeight());
	if (clientWidth <= 0.0f || clientHeight <= 0.0f) {
		return;
	}

	if (state.selectedIndex >= static_cast<int>(options.sprites.size()) ||
		(state.selectedIndex >= 0 && !options.sprites[state.selectedIndex].sprite)) {
		state.selectedIndex = -1;
		state.isDragging = false;
		state.activeAxis = -1;
	}

	const ImRect imageRect(
		ImVec2(rectX, rectY),
		ImVec2(rectX + rectWidth, rectY + rectHeight));
	const float screenScaleX = rectWidth / clientWidth;
	const float screenScaleY = rectHeight / clientHeight;
	const auto toScreen = [&](const Vector2& point) {
		return ImVec2(
			rectX + point.x * screenScaleX,
			rectY + point.y * screenScaleY);
	};

	// 3Dツールと重ならない位置に、2D専用の小さな操作パネルを表示します。
	ImGui::SetNextWindowPos(ImVec2(rectX + 10.0f, rectY + 135.0f), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.88f);
	const ImGuiWindowFlags toolbarFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking;
	ImGui::Begin("2D Edit Tools", nullptr, toolbarFlags);
	const auto drawToolButton = [&](const char* label, TransformTool tool) {
		const bool isActive = state.tool == tool;
		if (isActive) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.48f, 0.82f, 1.0f));
		}
		if (ImGui::Button(label)) {
			state.tool = tool;
			state.isDragging = false;
			state.activeAxis = -1;
		}
		if (isActive) {
			ImGui::PopStyleColor();
		}
	};
	ImGui::TextUnformatted("2D Sprite");
	drawToolButton("Move##Sprite", TransformTool::Move);
	ImGui::SameLine();
	drawToolButton("Rotate##Sprite", TransformTool::Rotate);
	ImGui::SameLine();
	drawToolButton("Size##Sprite", TransformTool::Scale);
	if (state.selectedIndex >= 0) {
		ImGui::Text("Selected: %s", options.sprites[state.selectedIndex].label.c_str());
	} else {
		ImGui::TextDisabled("Left click a 2D texture to select it.");
	}
	const bool toolbarHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
	ImGui::End();

	Sprite* selectedSprite = state.selectedIndex >= 0
		? options.sprites[state.selectedIndex].sprite
		: nullptr;
	bool gizmoHovered = false;
	if (selectedSprite) {
		const Vector2 position = selectedSprite->GetPosition();
		const Vector2 size = selectedSprite->GetSize();
		const Vector2 anchor = selectedSprite->GetAnchorPoint();
		const float rotation = selectedSprite->GetRotation();
		const float cosine = std::cos(rotation);
		const float sine = std::sin(rotation);
		const auto rotatePoint = [&](const Vector2& local) {
			return Vector2{
				position.x + local.x * cosine - local.y * sine,
				position.y + local.x * sine + local.y * cosine,
			};
		};
		const std::array<Vector2, 4> localCorners{ {
			{ -size.x * anchor.x, -size.y * anchor.y },
			{ size.x * (1.0f - anchor.x), -size.y * anchor.y },
			{ size.x * (1.0f - anchor.x), size.y * (1.0f - anchor.y) },
			{ -size.x * anchor.x, size.y * (1.0f - anchor.y) },
		} };
		std::array<ImVec2, 4> corners{};
		for (size_t index = 0; index < corners.size(); ++index) {
			corners[index] = toScreen(rotatePoint(localCorners[index]));
		}

		const Vector2 worldCenter = rotatePoint({
			size.x * (0.5f - anchor.x),
			size.y * (0.5f - anchor.y),
		});
		const ImVec2 center = toScreen(worldCenter);
		const ImVec2 mouse = ImGui::GetMousePos();
		constexpr float axisLength = 58.0f;
		const ImVec2 xHandle(center.x + axisLength, center.y);
		const ImVec2 yHandle(center.x, center.y - axisLength);
		const float halfScreenWidth = std::abs(size.x * screenScaleX) * 0.5f;
		const float halfScreenHeight = std::abs(size.y * screenScaleY) * 0.5f;
		const float rotationRadius = (std::max)({ halfScreenWidth, halfScreenHeight, 34.0f }) + 18.0f;
		const float centerDistance = std::sqrt(
			(mouse.x - center.x) * (mouse.x - center.x) +
			(mouse.y - center.y) * (mouse.y - center.y));

		int hoveredAxis = -1;
		if (state.tool == TransformTool::Rotate) {
			if (std::abs(centerDistance - rotationRadius) <= 10.0f) {
				hoveredAxis = 2;
			}
		} else {
			float bestDistance = 11.0f;
			const float xDistance = SceneEditorViewportMath::DistanceToSegment(mouse, center, xHandle);
			if (xDistance <= bestDistance) {
				bestDistance = xDistance;
				hoveredAxis = 0;
			}
			const float yDistance = SceneEditorViewportMath::DistanceToSegment(mouse, center, yHandle);
			if (yDistance <= bestDistance) {
				hoveredAxis = 1;
			}
		}
		gizmoHovered = hoveredAxis >= 0;

		if (gizmoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			state.activeAxis = hoveredAxis;
			state.isDragging = true;
			state.previousMouseAngle = std::atan2(mouse.y - center.y, mouse.x - center.x);
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			state.activeAxis = -1;
			state.isDragging = false;
		}

		if (state.isDragging && state.activeAxis >= 0) {
			const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
			Vector2 editedPosition = selectedSprite->GetPosition();
			Vector2 editedSize = selectedSprite->GetSize();
			float editedRotation = selectedSprite->GetRotation();
			if (state.tool == TransformTool::Move) {
				if (state.activeAxis == 0) {
					editedPosition.x += mouseDelta.x / screenScaleX;
				}
				if (state.activeAxis == 1) {
					editedPosition.y += mouseDelta.y / screenScaleY;
				}
				selectedSprite->SetPosition(editedPosition);
			} else if (state.tool == TransformTool::Rotate) {
				const float mouseAngle = std::atan2(mouse.y - center.y, mouse.x - center.x);
				float angleDelta = mouseAngle - state.previousMouseAngle;
				if (angleDelta > std::numbers::pi_v<float>) angleDelta -= std::numbers::pi_v<float> * 2.0f;
				if (angleDelta < -std::numbers::pi_v<float>) angleDelta += std::numbers::pi_v<float> * 2.0f;
				editedRotation += angleDelta;
				state.previousMouseAngle = mouseAngle;
				selectedSprite->SetRotation(editedRotation);
			} else {
				if (state.activeAxis == 0) {
					editedSize.x = (std::max)(1.0f, editedSize.x + mouseDelta.x / screenScaleX);
				}
				if (state.activeAxis == 1) {
					editedSize.y = (std::max)(1.0f, editedSize.y - mouseDelta.y / screenScaleY);
				}
				selectedSprite->SetSize(editedSize);
			}
			if (options.onTransformChanged) {
				options.onTransformChanged(
					state.selectedIndex,
					selectedSprite->GetPosition(),
					selectedSprite->GetRotation(),
					selectedSprite->GetSize());
			}
		}

		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->PushClipRect(imageRect.Min, imageRect.Max, true);
		for (size_t index = 0; index < corners.size(); ++index) {
			drawList->AddLine(
				corners[index],
				corners[(index + 1) % corners.size()],
				IM_COL32(255, 225, 95, 255),
				2.0f);
		}
		if (state.tool == TransformTool::Rotate) {
			drawList->AddCircle(
				center,
				rotationRadius,
				gizmoHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 190, 70, 255),
				48,
				3.0f);
			drawList->AddText(
				ImVec2(center.x + rotationRadius + 8.0f, center.y - 8.0f),
				IM_COL32(255, 220, 120, 255),
				"Rotate");
		} else {
			const ImU32 xColor = state.activeAxis == 0
				? IM_COL32(255, 255, 255, 255)
				: IM_COL32(255, 75, 75, 255);
			const ImU32 yColor = state.activeAxis == 1
				? IM_COL32(255, 255, 255, 255)
				: IM_COL32(70, 235, 110, 255);
			drawList->AddLine(center, xHandle, xColor, 4.0f);
			drawList->AddLine(center, yHandle, yColor, 4.0f);
			if (state.tool == TransformTool::Scale) {
				drawList->AddRectFilled(
					ImVec2(xHandle.x - 6.0f, xHandle.y - 6.0f),
					ImVec2(xHandle.x + 6.0f, xHandle.y + 6.0f),
					xColor);
				drawList->AddRectFilled(
					ImVec2(yHandle.x - 6.0f, yHandle.y - 6.0f),
					ImVec2(yHandle.x + 6.0f, yHandle.y + 6.0f),
					yColor);
			} else {
				drawList->AddCircleFilled(xHandle, 7.0f, xColor);
				drawList->AddCircleFilled(yHandle, 7.0f, yColor);
			}
			drawList->AddText(ImVec2(xHandle.x + 8.0f, xHandle.y - 8.0f), xColor, "X");
			drawList->AddText(ImVec2(yHandle.x + 8.0f, yHandle.y - 8.0f), yColor, "Y");
		}
		drawList->PopClipRect();
	}

	// ギズモ以外を左クリックした時だけ、手前に描かれたスプライトから順に選択します。
	if (!state.isDragging &&
		!gizmoHovered &&
		!toolbarHovered &&
		imageRect.Contains(ImGui::GetMousePos()) &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		const ImVec2 mouse = ImGui::GetMousePos();
		const Vector2 mouseInClient{
			(mouse.x - rectX) / screenScaleX,
			(mouse.y - rectY) / screenScaleY,
		};
		int selectedIndex = -1;
		for (int index = static_cast<int>(options.sprites.size()) - 1; index >= 0; --index) {
			Sprite* sprite = options.sprites[index].sprite;
			if (!sprite) {
				continue;
			}
			const Vector2 position = sprite->GetPosition();
			const Vector2 size = sprite->GetSize();
			const Vector2 anchor = sprite->GetAnchorPoint();
			const float rotation = sprite->GetRotation();
			const float cosine = std::cos(-rotation);
			const float sine = std::sin(-rotation);
			const Vector2 delta{
				mouseInClient.x - position.x,
				mouseInClient.y - position.y,
			};
			const Vector2 local{
				delta.x * cosine - delta.y * sine,
				delta.x * sine + delta.y * cosine,
			};
			const float minimumX = -size.x * anchor.x;
			const float minimumY = -size.y * anchor.y;
			if (local.x >= minimumX &&
				local.x <= minimumX + size.x &&
				local.y >= minimumY &&
				local.y <= minimumY + size.y) {
				selectedIndex = index;
				break;
			}
		}
		if (state.selectedIndex != selectedIndex) {
			state.selectedIndex = selectedIndex;
			if (options.onSelectionChanged) {
				options.onSelectionChanged(selectedIndex);
			}
		}
	}
#else
	(void)state;
	(void)options;
#endif
}
