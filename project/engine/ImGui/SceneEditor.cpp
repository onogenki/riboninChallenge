#include "SceneEditor.h"
#include "ImGuiManager.h"
#include "SceneEditorShelfMessages.h"
#include "SceneEditorViewportMath.h"
#include "TextureManager.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <unordered_map>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#endif

namespace {
	struct CachedModelBounds
	{
		Vector3 minimum{};
		Vector3 maximum{};
	};

	// Model Shelfに表示するモデル・Texture種別ごとの件数です。
	struct ShelfStatistics
	{
		size_t loadableModelCount = 0;
		size_t animationModelCount = 0;
		size_t textureCount = 0;
		size_t unsupportedModelCount = 0;
	};

	// 一覧を一度だけ走査し、Shelf上部へ表示する件数を集計します。
	ShelfStatistics CalculateShelfStatistics(const std::vector<SceneEditor::ShelfEntry>& entries)
	{
		ShelfStatistics statistics{};
		for (const SceneEditor::ShelfEntry& entry : entries) {
			if (entry.isTexture) {
				++statistics.textureCount;
			} else if (!entry.canLoad) {
				++statistics.unsupportedModelCount;
			} else {
				++statistics.loadableModelCount;
				if (entry.hasAnimation) {
					++statistics.animationModelCount;
				}
			}
		}
		return statistics;
	}

	// ShelfStateが保持する選択名から、実際の表示項目を返します。
	const SceneEditor::ShelfEntry* FindSelectedShelfEntry(const SceneEditor::ShelfState& state)
	{
		const auto selectedIt = std::find_if(
			state.entries.begin(),
			state.entries.end(),
			[&state](const SceneEditor::ShelfEntry& entry)
			{
				return entry.fileName == state.selectedEntry;
			});
		return selectedIt != state.entries.end() ? &(*selectedIt) : nullptr;
	}

	// モデルまたはTextureをSceneへ追加・Previewできる状態かを返します。
	bool IsShelfEntryLoadable(const SceneEditor::ShelfEntry* entry)
	{
		return entry != nullptr && (entry->canLoad || entry->isTexture);
	}

	// 削除済みObjectを選択したままにならないよう、Viewportの選択状態を安全に戻します。
	void ResetInvalidViewportSelection(
		SceneEditor::ViewportState& state,
		const std::vector<SceneEditor::ViewportObject>& objects)
	{
		if (state.selectedIndex == -1) {
			return;
		}
		if (state.selectedIndex >= 0 &&
			state.selectedIndex < static_cast<int>(objects.size()) &&
			objects[state.selectedIndex].object) {
			return;
		}

		state.selectedIndex = -1;
		state.isDragging = false;
		state.activeAxis = -1;
	}

	Vector3 TransformEditorPoint(const Vector3& point, const Matrix4x4& matrix)
	{
		return {
			point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0],
			point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1],
			point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2],
		};
	}

#ifdef USE_IMGUI
	bool ProjectEditorPoint(const Vector3& point, const Matrix4x4& viewProjection, const ImVec2& imageMin, const ImVec2& imageSize, ImVec2& screen)
	{
		const float x = point.x * viewProjection.m[0][0] + point.y * viewProjection.m[1][0] + point.z * viewProjection.m[2][0] + viewProjection.m[3][0];
		const float y = point.x * viewProjection.m[0][1] + point.y * viewProjection.m[1][1] + point.z * viewProjection.m[2][1] + viewProjection.m[3][1];
		const float w = point.x * viewProjection.m[0][3] + point.y * viewProjection.m[1][3] + point.z * viewProjection.m[2][3] + viewProjection.m[3][3];
		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) ||
			!std::isfinite(imageMin.x) || !std::isfinite(imageMin.y) ||
			!std::isfinite(imageSize.x) || !std::isfinite(imageSize.y) ||
			w <= 0.0001f) {
			return false;
		}
		screen = {
			imageMin.x + (x / w + 1.0f) * 0.5f * imageSize.x,
			imageMin.y + (1.0f - y / w) * 0.5f * imageSize.y,
		};
		return std::isfinite(screen.x) && std::isfinite(screen.y);
	}

	// Edit View左上のTransform切替とCamera編集を表示し、操作中ならtrueを返します。
	bool DrawViewportToolbar(
		SceneEditor::ViewportState& state,
		const SceneEditor::ViewportOptions& options,
		float rectX,
		float rectY)
	{
		ImGui::SetNextWindowPos(ImVec2(rectX + 10.0f, rectY + 10.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.88f);
		const ImGuiWindowFlags toolbarFlags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking;
		ImGui::Begin("Edit View Tools", nullptr, toolbarFlags);
		const auto drawToolButton = [&state](const char* label, SceneEditor::TransformTool tool) {
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
		drawToolButton("Move", SceneEditor::TransformTool::Move);
		ImGui::SameLine();
		drawToolButton("Rotate", SceneEditor::TransformTool::Rotate);
		ImGui::SameLine();
		drawToolButton("Scale", SceneEditor::TransformTool::Scale);
		if (state.selectedIndex >= 0) {
			ImGui::Text("Selected: %s", options.objects[state.selectedIndex].label.c_str());
		} else {
			ImGui::TextDisabled("Left click an object to select it.");
		}
		ImGui::TextDisabled("RMB: rotate camera | MMB: pan | Wheel: zoom");
		if (ImGui::TreeNode("Camera Transform")) {
			Vector3 cameraPosition = options.camera->GetTranslate();
			if (ImGui::DragFloat3("Camera Position", &cameraPosition.x, 0.05f)) {
				options.camera->SetTranslate(cameraPosition);
			}
			constexpr float kRadianToDegree = 180.0f / std::numbers::pi_v<float>;
			constexpr float kDegreeToRadian = std::numbers::pi_v<float> / 180.0f;
			const Vector3 cameraRotation = options.camera->GetRotate();
			float cameraRotationDegrees[3]{
				cameraRotation.x * kRadianToDegree,
				cameraRotation.y * kRadianToDegree,
				cameraRotation.z * kRadianToDegree,
			};
			if (ImGui::DragFloat3("Camera Rotation (deg)", cameraRotationDegrees, 1.0f)) {
				options.camera->SetRotate({
					cameraRotationDegrees[0] * kDegreeToRadian,
					cameraRotationDegrees[1] * kDegreeToRadian,
					cameraRotationDegrees[2] * kDegreeToRadian,
				});
			}
			ImGui::TreePop();
		}
		const bool toolbarHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		ImGui::End();
		return toolbarHovered;
	}

#endif

	bool BuildEditorWorldBounds(const Object3d& object, Vector3& minimum, Vector3& maximum)
	{
		Model* model = object.GetModel();
		if (!model || model->GetModelData().vertices.empty()) {
			return false;
		}

		static std::unordered_map<const Model*, CachedModelBounds> cachedBounds;
		auto cached = cachedBounds.find(model);
		if (cached == cachedBounds.end()) {
			CachedModelBounds bounds{};
			bounds.minimum = {
				(std::numeric_limits<float>::max)(),
				(std::numeric_limits<float>::max)(),
				(std::numeric_limits<float>::max)(),
			};
			bounds.maximum = {
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
			};
			for (const Model::VertexData& vertex : model->GetModelData().vertices) {
				const Vector3 point{ vertex.position.x, vertex.position.y, vertex.position.z };
				bounds.minimum.x = (std::min)(bounds.minimum.x, point.x);
				bounds.minimum.y = (std::min)(bounds.minimum.y, point.y);
				bounds.minimum.z = (std::min)(bounds.minimum.z, point.z);
				bounds.maximum.x = (std::max)(bounds.maximum.x, point.x);
				bounds.maximum.y = (std::max)(bounds.maximum.y, point.y);
				bounds.maximum.z = (std::max)(bounds.maximum.z, point.z);
			}
			cached = cachedBounds.emplace(model, bounds).first;
		}

		const CachedModelBounds& local = cached->second;
		const Transform& transform = object.GetTransform();
		const Matrix4x4 world = MyMath::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
		const std::array<Vector3, 8> corners{ {
			{ local.minimum.x, local.minimum.y, local.minimum.z },
			{ local.maximum.x, local.minimum.y, local.minimum.z },
			{ local.minimum.x, local.maximum.y, local.minimum.z },
			{ local.maximum.x, local.maximum.y, local.minimum.z },
			{ local.minimum.x, local.minimum.y, local.maximum.z },
			{ local.maximum.x, local.minimum.y, local.maximum.z },
			{ local.minimum.x, local.maximum.y, local.maximum.z },
			{ local.maximum.x, local.maximum.y, local.maximum.z },
		} };

		minimum = {
			(std::numeric_limits<float>::max)(),
			(std::numeric_limits<float>::max)(),
			(std::numeric_limits<float>::max)(),
		};
		maximum = {
			std::numeric_limits<float>::lowest(),
			std::numeric_limits<float>::lowest(),
			std::numeric_limits<float>::lowest(),
		};
		for (const Vector3& corner : corners) {
			const Vector3 point = TransformEditorPoint(corner, world);
			if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
				return false;
			}
			minimum.x = (std::min)(minimum.x, point.x);
			minimum.y = (std::min)(minimum.y, point.y);
			minimum.z = (std::min)(minimum.z, point.z);
			maximum.x = (std::max)(maximum.x, point.x);
			maximum.y = (std::max)(maximum.y, point.y);
			maximum.z = (std::max)(maximum.z, point.z);
		}
		return true;
	}

}

void SceneEditor::DrawModelShelf(ShelfState& state, const ShelfCallbacks& callbacks)
{
#ifdef USE_IMGUI
	// リソース棚は配置作業専用なので、Game Viewでは生成しない。
	if (!ImGuiManager::GetInstance()->IsEditViewActive()) {
		return;
	}

	if (!ImGui::Begin("Model Shelf")) {
		ImGui::End();
		return;
	}

	ShelfStatistics statistics = CalculateShelfStatistics(state.entries);
	const ShelfEntry* selectedEntry = FindSelectedShelfEntry(state);
	bool hasLoadableSelection = IsShelfEntryLoadable(selectedEntry);

	ImGui::Text("%s Resources (%zu)", callbacks.sceneLabel.c_str(), state.entries.size());
	ImGui::SameLine();
	if (ImGui::SmallButton("Refresh")) {
		ScanResourceShelf(state);
		statistics = CalculateShelfStatistics(state.entries);
		selectedEntry = FindSelectedShelfEntry(state);
		hasLoadableSelection = IsShelfEntryLoadable(selectedEntry);
		state.message = "Refreshed " + callbacks.sceneLabel + " resources: " + std::to_string(state.entries.size()) + " shelf item(s) found.";
	}
	if (callbacks.drawExtraToolbar) {
		callbacks.drawExtraToolbar();
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(callbacks.addedModelCount == 0 && callbacks.addedTextureCount == 0 || !callbacks.clearAdded);
	if (ImGui::SmallButton(("Clear " + callbacks.sceneLabel + " Added").c_str())) {
		callbacks.clearAdded();
		state.message = "Cleared " + callbacks.sceneLabel + " added models and textures.";
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("Added: %zu (Textures: %zu)", callbacks.addedModelCount + callbacks.addedTextureCount, callbacks.addedTextureCount);
	ImGui::TextDisabled(
		"Models: %zu | Animation: %zu | 2D Textures: %zu | Unsupported: %zu",
		statistics.loadableModelCount,
		statistics.animationModelCount,
		statistics.textureCount,
		statistics.unsupportedModelCount);
	if (!state.message.empty()) {
		ImGui::TextWrapped("%s", state.message.c_str());
	}
	if (callbacks.drawExtraStatus) {
		callbacks.drawExtraStatus();
	}

	if (callbacks.previewEntry) {
		ImGui::BeginDisabled(!hasLoadableSelection);
		if (ImGui::SmallButton("Preview Selected")) {
			const bool success = callbacks.previewEntry(*selectedEntry);
			state.message = SceneEditorShelfMessages::MakePreviewMessage(selectedEntry->isTexture ? "2D Texture" : "model", selectedEntry->isTexture ? selectedEntry->textureFilePath : selectedEntry->fileName, success);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
	}
	ImGui::BeginDisabled(!hasLoadableSelection);
	if (ImGui::SmallButton(("Add Selected to " + callbacks.sceneLabel).c_str())) {
		if (selectedEntry->isTexture) {
			const bool success = callbacks.addTexture && callbacks.addTexture(selectedEntry->textureFilePath);
			state.message = SceneEditorShelfMessages::MakeAddMessage(callbacks.sceneLabel, "2D Texture", selectedEntry->textureFilePath, success);
		} else {
			const bool success = callbacks.addModel && callbacks.addModel(selectedEntry->fileName);
			state.message = SceneEditorShelfMessages::MakeAddMessage(callbacks.sceneLabel, "model", selectedEntry->fileName, success);
		}
		if (callbacks.afterAdd) {
			callbacks.afterAdd();
		}
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip(hasLoadableSelection ? "Add the selected card to the scene." : "Select a loadable model or texture card first.");
	}
	ImGui::Separator();

	constexpr int cardsPerRow = 6;
	ImGui::BeginChild("SceneEditorModelShelfGrid", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoMove);
	const float shelfSpacingX = ImGui::GetStyle().ItemSpacing.x;
	const float shelfAvailableWidth = (std::max)(1.0f, ImGui::GetContentRegionAvail().x);
	const float calculatedCardWidth = (shelfAvailableWidth - shelfSpacingX * static_cast<float>(cardsPerRow - 1)) / static_cast<float>(cardsPerRow);
	const ImVec2 cardSize((std::max)(118.0f, (std::min)(150.0f, calculatedCardWidth)), 118.0f);

	if (state.entries.empty()) {
		ImGui::TextDisabled("No supported model or texture files were found under resources.");
	}

	size_t cardIndex = 0;
	for (const ShelfEntry& entry : state.entries) {
		if (cardIndex > 0 && cardIndex % cardsPerRow != 0) {
			ImGui::SameLine();
		}
		ImGui::PushID(entry.fileName.c_str());
		const ImVec2 cardMin = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("SceneEditorModelCard", cardSize);
		const bool hovered = ImGui::IsItemHovered();
		const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		const bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		const bool selected = state.selectedEntry == entry.fileName;

		if (clicked) {
			state.selectedEntry = entry.fileName;
		}
		if (doubleClicked && entry.isTexture) {
			state.selectedEntry = entry.fileName;
			if (callbacks.previewOnDoubleClick && callbacks.previewEntry) {
				const bool success = callbacks.previewEntry(entry);
				state.message = SceneEditorShelfMessages::MakePreviewMessage("2D Texture", entry.textureFilePath, success);
			} else {
				const bool success = callbacks.addTexture && callbacks.addTexture(entry.textureFilePath);
				state.message = SceneEditorShelfMessages::MakeAddMessage(callbacks.sceneLabel, "2D Texture", entry.textureFilePath, success);
				if (callbacks.afterAdd) {
					callbacks.afterAdd();
				}
			}
		} else if (doubleClicked && entry.canLoad) {
			state.selectedEntry = entry.fileName;
			if (callbacks.previewOnDoubleClick && callbacks.previewEntry) {
				const bool success = callbacks.previewEntry(entry);
				state.message = SceneEditorShelfMessages::MakePreviewMessage("model", entry.fileName, success);
			} else {
				const bool success = callbacks.addModel && callbacks.addModel(entry.fileName);
				state.message = SceneEditorShelfMessages::MakeAddMessage(callbacks.sceneLabel, "model", entry.fileName, success);
				if (callbacks.afterAdd) {
					callbacks.afterAdd();
				}
			}
		}

		if (entry.isTexture && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("TEXTURE_FILE", entry.textureFilePath.c_str(), entry.textureFilePath.size() + 1);
			ImGui::Text("Add 2D Texture %s", entry.textureFilePath.c_str());
			ImGui::EndDragDropSource();
		} else if (entry.canLoad && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("MODEL_FILE", entry.fileName.c_str(), entry.fileName.size() + 1);
			ImGui::Text("Add %s", entry.fileName.c_str());
			ImGui::EndDragDropSource();
		}

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 cardMax(cardMin.x + cardSize.x, cardMin.y + cardSize.y);
		const ImU32 backgroundColor = selected ? IM_COL32(64, 88, 130, 255) : (hovered ? IM_COL32(50, 50, 56, 255) : IM_COL32(34, 34, 38, 255));
		const ImU32 borderColor = entry.isTexture
			? IM_COL32(120, 210, 170, 255)
			: (entry.canLoad ? IM_COL32(120, 150, 210, 255) : IM_COL32(120, 80, 80, 255));
		drawList->AddRectFilled(cardMin, cardMax, backgroundColor, 8.0f);
		drawList->AddRect(cardMin, cardMax, borderColor, 8.0f, 0, selected ? 3.0f : 1.5f);

		const ImVec2 previewMin(cardMin.x + 10.0f, cardMin.y + 10.0f);
		const ImVec2 previewMax(cardMax.x - 10.0f, cardMin.y + 76.0f);
		drawList->AddRectFilled(previewMin, previewMax, IM_COL32(18, 18, 22, 255), 5.0f);
		drawList->AddRect(previewMin, previewMax, IM_COL32(78, 78, 92, 255), 5.0f);

		const float centerX = (previewMin.x + previewMax.x) * 0.5f;
		const float centerY = (previewMin.y + previewMax.y) * 0.5f;
		const float animationPulse = entry.hasAnimation && hovered ? (std::sin(static_cast<float>(ImGui::GetTime()) * 8.0f) * 0.5f + 0.5f) : 0.0f;
		const ImU32 modelColor = entry.hasAnimation
			? IM_COL32(255, static_cast<int>(190.0f + 50.0f * animationPulse), 70, 255)
			: IM_COL32(120, 180, 255, 255);
		if (entry.isTexture) {
			const float previewWidth = previewMax.x - previewMin.x;
			const float previewHeight = previewMax.y - previewMin.y;
			const float textureWidth = (std::max)(entry.textureSize.x, 1.0f);
			const float textureHeight = (std::max)(entry.textureSize.y, 1.0f);
			const float imageScale = (std::min)(previewWidth / textureWidth, previewHeight / textureHeight);
			const ImVec2 imageSize(textureWidth * imageScale, textureHeight * imageScale);
			const ImVec2 imageMin(previewMin.x + (previewWidth - imageSize.x) * 0.5f, previewMin.y + (previewHeight - imageSize.y) * 0.5f);
			const ImVec2 imageMax(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
			D3D12_GPU_DESCRIPTOR_HANDLE textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(entry.textureFilePath);
			drawList->PushClipRect(previewMin, previewMax, true);
			drawList->AddImage(ImTextureRef(static_cast<ImTextureID>(textureHandle.ptr)), imageMin, imageMax);
			drawList->PopClipRect();
			drawList->AddText(ImVec2(previewMax.x - 32.0f, previewMin.y + 8.0f), IM_COL32(130, 255, 190, 230), "2D");
		} else if (!entry.thumbnailLines.empty() || !entry.thumbnailTriangles.empty()) {
			const float yaw = entry.hasAnimation && hovered ? static_cast<float>(ImGui::GetTime()) * 2.2f : 0.65f;
			const float cosYaw = std::cos(yaw);
			const float sinYaw = std::sin(yaw);
			const float previewWidth = previewMax.x - previewMin.x;
			const float previewHeight = previewMax.y - previewMin.y;
			const float previewScale = (std::min)(previewWidth, previewHeight) * (0.35f + 0.035f * animationPulse) / entry.thumbnailRadius;
			const auto projectThumbnailPoint = [&](const Vector3& position) {
				const Vector3 local{
					position.x - entry.thumbnailCenter.x,
					position.y - entry.thumbnailCenter.y,
					position.z - entry.thumbnailCenter.z
				};
				const float animationTime = static_cast<float>(ImGui::GetTime());
				const float hoverWave = entry.hasAnimation && hovered
					? std::sin(animationTime * 7.0f + local.x * 2.3f + local.z * 1.4f) * entry.thumbnailRadius * 0.08f
					: 0.0f;
				const float hoverStride = entry.hasAnimation && hovered
					? std::sin(animationTime * 5.0f + local.y * 3.0f) * entry.thumbnailRadius * 0.04f
					: 0.0f;
				const float animatedX = local.x + hoverStride;
				const float animatedY = local.y + hoverWave;
				const float rotatedX = animatedX * cosYaw + local.z * sinYaw;
				const float rotatedZ = -animatedX * sinYaw + local.z * cosYaw;
				return ImVec2(centerX + rotatedX * previewScale, centerY - animatedY * previewScale + rotatedZ * previewScale * 0.18f);
			};

			drawList->PushClipRect(previewMin, previewMax, true);
			const ImU32 fillColor = entry.hasAnimation
				? IM_COL32(255, static_cast<int>(150.0f + 45.0f * animationPulse), 45, 95)
				: IM_COL32(80, 145, 255, 95);
			for (const auto& triangle : entry.thumbnailTriangles) {
				drawList->AddTriangleFilled(projectThumbnailPoint(triangle[0]), projectThumbnailPoint(triangle[1]), projectThumbnailPoint(triangle[2]), fillColor);
			}
			for (const auto& line : entry.thumbnailLines) {
				drawList->AddLine(projectThumbnailPoint(line.first), projectThumbnailPoint(line.second), modelColor, 1.15f);
			}
			if (entry.hasAnimation) {
				drawList->AddCircleFilled(ImVec2(previewMax.x - 10.0f, previewMin.y + 10.0f), 3.0f + 1.5f * animationPulse, IM_COL32(255, 210, 80, 230));
			}
			drawList->PopClipRect();
		} else {
			drawList->AddTriangleFilled(ImVec2(centerX, centerY - 18.0f - 4.0f * animationPulse), ImVec2(centerX - 24.0f, centerY + 18.0f), ImVec2(centerX + 24.0f, centerY + 18.0f), modelColor);
		}

		const auto drawCardText = [&](const ImVec2& position, ImU32 color, const char* text) {
			const ImVec2 clipMin(cardMin.x + 8.0f, position.y);
			const ImVec2 clipMax(cardMax.x - 8.0f, position.y + ImGui::GetTextLineHeight());
			drawList->PushClipRect(clipMin, clipMax, true);
			drawList->AddText(position, color, text);
			drawList->PopClipRect();
		};
		drawCardText(ImVec2(cardMin.x + 10.0f, cardMin.y + 82.0f), IM_COL32(235, 235, 235, 255), entry.displayName.c_str());
		const char* typeLabel = entry.isTexture ? "TEXTURE" : (entry.hasAnimation ? "ANIM" : "MODEL");
		const ImU32 typeColor = entry.isTexture
			? IM_COL32(140, 255, 190, 255)
			: (entry.canLoad ? IM_COL32(160, 210, 255, 255) : IM_COL32(255, 140, 140, 255));
		drawCardText(ImVec2(cardMin.x + 10.0f, cardMin.y + 100.0f), typeColor, entry.canLoad || entry.isTexture ? typeLabel : "UNSUPPORTED");
		if (!entry.canLoad && !entry.isTexture) {
			drawList->AddText(ImVec2(previewMin.x + 10.0f, centerY - 7.0f), IM_COL32(255, 120, 120, 230), "Cannot load");
		}

		if (hovered) {
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(entry.fileName.c_str());
			if (entry.isTexture) {
				ImGui::Text("2D Texture preview");
				ImGui::Text("Size: %.0f x %.0f", entry.textureSize.x, entry.textureSize.y);
				ImGui::Text("Double click: add to scene");
			} else if (entry.canLoad) {
				ImGui::Text("Double click: add to scene");
				ImGui::Text("Right Inspector: adjust after adding");
			} else {
				ImGui::Text("This file was found, but no drawable mesh could be loaded.");
			}
			if (entry.hasAnimation) {
				ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "Animation model");
			}
			if (!entry.canLoad && !entry.isTexture) {
				ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "No mesh found");
			}
			ImGui::EndTooltip();
		}

		ImGui::PopID();
		++cardIndex;
	}

	ImGui::EndChild();
	ImGui::End();
#else
	(void)state;
	(void)callbacks;
#endif
}

void SceneEditor::DrawViewportEditor(ViewportState& state, const ViewportOptions& options)
{
#ifdef USE_IMGUI
	if (!ImGuiManager::GetInstance()->IsEditViewActive() || !options.camera) {
		return;
	}

	float rectX = 0.0f;
	float rectY = 0.0f;
	float rectWidth = 0.0f;
	float rectHeight = 0.0f;
	if (!ImGuiManager::GetInstance()->GetGameViewRect(rectX, rectY, rectWidth, rectHeight)) {
		return;
	}
	if (!std::isfinite(rectX) || !std::isfinite(rectY) ||
		!std::isfinite(rectWidth) || !std::isfinite(rectHeight) ||
		rectWidth <= 1.0f || rectHeight <= 1.0f) {
		return;
	}
	const ImVec2 imageMin(rectX, rectY);
	const ImVec2 imageSize(rectWidth, rectHeight);
	const ImRect imageRect(imageMin, ImVec2(rectX + rectWidth, rectY + rectHeight));
	const Matrix4x4& viewProjection = options.camera->GetViewProjectionMatrix();

	ResetInvalidViewportSelection(state, options.objects);

	// Transform切替とCamera編集は、Viewport本体とは別の小さな操作パネルへ任せます。
	const bool toolbarHovered = DrawViewportToolbar(state, options, rectX, rectY);

	Object3d* selectedObject = state.selectedIndex >= 0
		? options.objects[state.selectedIndex].object
		: nullptr;
	bool gizmoHovered = false;
	bool transformChanged = false;
	Transform changedTransform{};
	int changedTransformIndex = -1;
	if (selectedObject) {
		Vector3 boundsMin{};
		Vector3 boundsMax{};
		if (BuildEditorWorldBounds(*selectedObject, boundsMin, boundsMax)) {
			const Vector3 center{
				(boundsMin.x + boundsMax.x) * 0.5f,
				(boundsMin.y + boundsMax.y) * 0.5f,
				(boundsMin.z + boundsMax.z) * 0.5f,
			};
			const Vector3 boundsSize{
				boundsMax.x - boundsMin.x,
				boundsMax.y - boundsMin.y,
				boundsMax.z - boundsMin.z,
			};
			const float axisLength = (std::max)({ boundsSize.x, boundsSize.y, boundsSize.z, 1.0f }) * 0.85f;
			const std::array<Vector3, 6> axisEnds{ {
				{ center.x + axisLength, center.y, center.z },
				{ center.x - axisLength, center.y, center.z },
				{ center.x, center.y + axisLength, center.z },
				{ center.x, center.y - axisLength, center.z },
				{ center.x, center.y, center.z + axisLength },
				{ center.x, center.y, center.z - axisLength },
			} };
			const std::array<Vector3, 6> worldDirections{ {
				{ 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
				{ 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f },
				{ 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f },
			} };
			ImVec2 centerScreen{};
			std::array<ImVec2, 6> endScreens{};
			std::array<bool, 6> isAxisEndVisible{};
			if (ProjectEditorPoint(center, viewProjection, imageMin, imageSize, centerScreen)) {
				const ImVec2 mouse = ImGui::GetMousePos();
				int hoveredAxisEnd = -1;
				float bestDistance = 11.0f;
				for (int axisEndIndex = 0; axisEndIndex < 6; ++axisEndIndex) {
					isAxisEndVisible[axisEndIndex] = ProjectEditorPoint(
						axisEnds[axisEndIndex],
						viewProjection,
						imageMin,
						imageSize,
						endScreens[axisEndIndex]);
					if (!isAxisEndVisible[axisEndIndex]) {
						continue;
					}
					const float distance = SceneEditorViewportMath::DistanceToSegment(mouse, centerScreen, endScreens[axisEndIndex]);
					if (distance <= bestDistance) {
						bestDistance = distance;
						hoveredAxisEnd = axisEndIndex;
					}
				}
				gizmoHovered = hoveredAxisEnd >= 0;

				if (gizmoHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					const ImVec2 direction(
						endScreens[hoveredAxisEnd].x - centerScreen.x,
						endScreens[hoveredAxisEnd].y - centerScreen.y);
					const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
					state.dragScreenDirectionX = length > 0.0001f ? direction.x / length : 0.0f;
					state.dragScreenDirectionY = length > 0.0001f ? direction.y / length : 0.0f;
					state.dragWorldDirection = worldDirections[hoveredAxisEnd];
					state.activeAxis = hoveredAxisEnd / 2;
					state.isDragging = true;
				}
				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					state.isDragging = false;
					state.activeAxis = -1;
				}

				if (state.isDragging && state.activeAxis >= 0) {
					const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
					const float dragAmount =
						mouseDelta.x * state.dragScreenDirectionX +
						mouseDelta.y * state.dragScreenDirectionY;
					if (std::abs(dragAmount) > 0.0001f) {
						Transform transform = selectedObject->GetTransform();
						const Transform beforeEdit = transform;
						if (state.tool == TransformTool::Move) {
							const float amount = dragAmount * (std::max)(axisLength, 1.0f) * 0.006f;
							transform.translate.x += state.dragWorldDirection.x * amount;
							transform.translate.y += state.dragWorldDirection.y * amount;
							transform.translate.z += state.dragWorldDirection.z * amount;
						} else if (state.tool == TransformTool::Rotate) {
							const float directionSign = state.dragWorldDirection.x + state.dragWorldDirection.y + state.dragWorldDirection.z;
							if (state.activeAxis == 0) transform.rotate.x += dragAmount * directionSign * 0.01f;
							if (state.activeAxis == 1) transform.rotate.y += dragAmount * directionSign * 0.01f;
							if (state.activeAxis == 2) transform.rotate.z += dragAmount * directionSign * 0.01f;
						} else {
							if (state.activeAxis == 0) transform.scale.x = std::clamp(transform.scale.x + dragAmount * 0.01f, 0.01f, 100.0f);
							if (state.activeAxis == 1) transform.scale.y = std::clamp(transform.scale.y + dragAmount * 0.01f, 0.01f, 100.0f);
							if (state.activeAxis == 2) transform.scale.z = std::clamp(transform.scale.z + dragAmount * 0.01f, 0.01f, 100.0f);
						}
						if (!std::isfinite(transform.translate.x) || !std::isfinite(transform.translate.y) || !std::isfinite(transform.translate.z) ||
							!std::isfinite(transform.rotate.x) || !std::isfinite(transform.rotate.y) || !std::isfinite(transform.rotate.z) ||
							!std::isfinite(transform.scale.x) || !std::isfinite(transform.scale.y) || !std::isfinite(transform.scale.z)) {
							state.isDragging = false;
							state.activeAxis = -1;
							return;
						}
						selectedObject->SetTranslate(transform.translate);
						selectedObject->SetRotate(transform.rotate);
						selectedObject->SetScale(transform.scale);
						selectedObject->RecordTransformEdit(beforeEdit);
						transformChanged = true;
						changedTransform = transform;
						changedTransformIndex = state.selectedIndex;
					}
				}

				ImDrawList* drawList = ImGui::GetForegroundDrawList();
				drawList->PushClipRect(imageRect.Min, imageRect.Max, true);
				const std::array<ImU32, 3> axisColors{
					IM_COL32(255, 75, 75, 255),
					IM_COL32(70, 235, 110, 255),
					IM_COL32(70, 145, 255, 255),
				};
				for (int axisEndIndex = 0; axisEndIndex < 6; ++axisEndIndex) {
					if (!isAxisEndVisible[axisEndIndex]) {
						continue;
					}
					const int axis = axisEndIndex / 2;
					ImU32 color = axisColors[axis];
					if (state.isDragging && state.activeAxis == axis) color = IM_COL32(255, 255, 255, 255);
					if (!state.isDragging && hoveredAxisEnd == axisEndIndex) color = IM_COL32(255, 225, 90, 255);
					drawList->AddLine(centerScreen, endScreens[axisEndIndex], color, axisEndIndex % 2 == 0 ? 4.0f : 2.0f);
					if (state.tool == TransformTool::Scale) {
						drawList->AddRectFilled(
							ImVec2(endScreens[axisEndIndex].x - 5.0f, endScreens[axisEndIndex].y - 5.0f),
							ImVec2(endScreens[axisEndIndex].x + 5.0f, endScreens[axisEndIndex].y + 5.0f),
							color);
					} else {
						drawList->AddCircleFilled(endScreens[axisEndIndex], 6.0f, color);
					}
				}
				const char* toolLabel = state.tool == TransformTool::Move
					? "Move XYZ"
					: (state.tool == TransformTool::Rotate ? "Rotate XYZ" : "Scale XYZ");
				drawList->AddText(ImVec2(centerScreen.x + 10.0f, centerScreen.y + 10.0f), IM_COL32(255, 255, 255, 240), toolLabel);
				drawList->PopClipRect();
			}
		}
	}

	// ギズモ以外を左クリックした時だけ、新しいオブジェクトを選択する。
	if (!state.isDragging && !gizmoHovered && !toolbarHovered &&
		imageRect.Contains(ImGui::GetMousePos()) &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		const ImVec2 mouse = ImGui::GetMousePos();
		float bestScore = (std::numeric_limits<float>::max)();
		int bestIndex = -1;
		for (int index = 0; index < static_cast<int>(options.objects.size()); ++index) {
			Object3d* object = options.objects[index].object;
			Vector3 boundsMin{};
			Vector3 boundsMax{};
			if (!object || !BuildEditorWorldBounds(*object, boundsMin, boundsMax)) {
				continue;
			}
			const std::array<Vector3, 8> corners{ {
				{ boundsMin.x, boundsMin.y, boundsMin.z }, { boundsMax.x, boundsMin.y, boundsMin.z },
				{ boundsMin.x, boundsMax.y, boundsMin.z }, { boundsMax.x, boundsMax.y, boundsMin.z },
				{ boundsMin.x, boundsMin.y, boundsMax.z }, { boundsMax.x, boundsMin.y, boundsMax.z },
				{ boundsMin.x, boundsMax.y, boundsMax.z }, { boundsMax.x, boundsMax.y, boundsMax.z },
			} };
			ImVec2 screenMin((std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)());
			ImVec2 screenMax(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
			int visibleCount = 0;
			for (const Vector3& corner : corners) {
				ImVec2 screen{};
				if (!ProjectEditorPoint(corner, viewProjection, imageMin, imageSize, screen)) continue;
				screenMin.x = (std::min)(screenMin.x, screen.x);
				screenMin.y = (std::min)(screenMin.y, screen.y);
				screenMax.x = (std::max)(screenMax.x, screen.x);
				screenMax.y = (std::max)(screenMax.y, screen.y);
				++visibleCount;
			}
			if (visibleCount == 0 || !ImRect(screenMin, screenMax).Contains(mouse)) continue;
			const float centerX = (screenMin.x + screenMax.x) * 0.5f;
			const float centerY = (screenMin.y + screenMax.y) * 0.5f;
			const float dx = mouse.x - centerX;
			const float dy = mouse.y - centerY;
			const float area = (std::max)(1.0f, (screenMax.x - screenMin.x) * (screenMax.y - screenMin.y));
			const float score = dx * dx + dy * dy + area * 0.01f;
			if (score < bestScore) {
				bestScore = score;
				bestIndex = index;
			}
		}
		if (state.selectedIndex != bestIndex) {
			state.selectedIndex = bestIndex;
			if (options.onSelectionChanged) {
				options.onSelectionChanged(bestIndex);
			}
		}
	}

	// 左クリックは選択・ギズモ専用とし、カメラは右・中ボタンへ分離して誤操作を防ぐ。
	if (transformChanged && options.onTransformChanged) {
		options.onTransformChanged(changedTransformIndex, changedTransform);
	}

	UpdateViewportCamera(options.camera, state.isDragging || toolbarHovered);
#else
	(void)state;
	(void)options;
#endif
}

