#include "ImGuiManager.h"
#include "CameraManager.h"
#include "Object3d.h"
#include "ParticleEmitter.h"
#include "ParticleManager.h"
#include "Sprite.h"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>

#ifdef USE_IMGUI
namespace
{
	// 調整した座標・サイズ・基準点を、計算式なしのC++コードとしてコピーします。
	void DrawSpritePlacementCopy(const Sprite& sprite)
	{
		if (ImGui::Button("Copy Placement C++")) {
			const Vector2& position = sprite.GetPosition();
			const Vector2& size = sprite.GetSize();
			const Vector2& anchor = sprite.GetAnchorPoint();
			char code[1024]{};
			std::snprintf(code, sizeof(code),
				"// Inspectorで調整した配置値です。spriteは配置したいSpriteに置き換えます。\n"
				"sprite->SetPosition({ %.6ff, %.6ff });\n"
				"sprite->SetSize({ %.6ff, %.6ff });\n"
				"sprite->SetAnchorPoint({ %.6ff, %.6ff });\n"
				"sprite->SetRotation(%.6ff);\n",
				position.x, position.y, size.x, size.y, anchor.x, anchor.y, sprite.GetRotation());
			ImGui::SetClipboardText(code);
		}
		ImGui::TextDisabled("Pos / Size: render pixels. Ctrl+click a value to type it.");
	}
}
#endif

// Sprite・Model・Particle・Cameraを編集する共通Inspector UIです。
int ImGuiManager::SpriteWindow(const std::vector<std::unique_ptr<Sprite>>& sprites, bool embedded, int forcedSpriteIndex)
{
#ifdef USE_IMGUI

	static int selectedSpriteIndex = 0;

	
	if (!embedded && !showSpriteWindow_) {
		return -1;
	}
	if (!embedded && !ImGui::Begin("Editing UVTranslate ( Sprite )", &showSpriteWindow_)) {
		ImGui::End();
		return -1;
	}
	ImGui::Separator();

	// 選択したSpriteの現在色を読み、他のSpriteの編集値を表示しないようにします。
	if (forcedSpriteIndex >= 0 && forcedSpriteIndex < static_cast<int>(sprites.size())) {
		selectedSpriteIndex = forcedSpriteIndex;
		Sprite* targetSprite = sprites[forcedSpriteIndex].get();
		if (targetSprite) {
			ImGui::Text("Selected 2D Texture / Sprite %d", forcedSpriteIndex);
			Vector4 color = targetSprite->GetColor();
			if (ImGui::ColorEdit4("Color", &color.x)) {
				targetSprite->SetColor(color);
			}
			Vector2 pos = targetSprite->GetPosition();
			if (ImGui::DragFloat2("Pos", &pos.x, 1.0f)) {
				targetSprite->SetPosition(pos);
			}
			float rot = targetSprite->GetRotation();
			if (ImGui::DragFloat("Rot", &rot, 0.01f)) {
				targetSprite->SetRotation(rot);
			}
			Vector2 size = targetSprite->GetSize();
			if (ImGui::DragFloat2("Size", &size.x, 1.0f)) {
				targetSprite->SetSize(size);
			}
			Vector2 anchor = targetSprite->GetAnchorPoint();
			if (ImGui::DragFloat2("Anchor", &anchor.x, 0.01f, 0.0f, 1.0f)) {
				targetSprite->SetAnchorPoint(anchor);
			}
			bool isFlipX = targetSprite->GetIsFlipX();
			if (ImGui::Checkbox("isFlipX", &isFlipX)) {
				targetSprite->SetIsFlipX(isFlipX);
			}
			bool isFlipY = targetSprite->GetIsFlipY();
			if (ImGui::Checkbox("isFlipY", &isFlipY)) {
				targetSprite->SetIsFlipY(isFlipY);
			}
			Vector2 texBase = targetSprite->GetTextureLeftTop();
			if (ImGui::DragFloat2("TexLeftTop", &texBase.x, 1.0f)) {
				targetSprite->SetTextureLeftTop(texBase);
			}
			Vector2 texSize = targetSprite->GetTextureSize();
			if (ImGui::DragFloat2("TexSize", &texSize.x, 1.0f)) {
				targetSprite->SetTextureSize(texSize);
			}
			DrawSpritePlacementCopy(*targetSprite);
		}
		if (!embedded) {
			ImGui::End();
		}
		return forcedSpriteIndex;
	}

	if (sprites.empty()) {
		ImGui::TextDisabled("No Sprite / 2D Texture objects.");
		if (!embedded) {
			ImGui::End();
		}
		return -1;
	}
	if (selectedSpriteIndex < 0 || selectedSpriteIndex >= static_cast<int>(sprites.size())) {
		selectedSpriteIndex = 0;
	}
	std::string spritePreview = "Sprite " + std::to_string(selectedSpriteIndex);
	if (ImGui::BeginCombo("Target", spritePreview.c_str())) {
		for (int i = 0; i < static_cast<int>(sprites.size()); ++i) {
			std::string item = "Sprite " + std::to_string(i);
			if (ImGui::Selectable(item.c_str(), selectedSpriteIndex == i)) {
				selectedSpriteIndex = i;
			}
			if (selectedSpriteIndex == i) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (sprites[selectedSpriteIndex]) {
		Vector4 color = sprites[selectedSpriteIndex]->GetColor();
		if (ImGui::ColorEdit4("Color", &color.x)) {
			sprites[selectedSpriteIndex]->SetColor(color);
		}
	}

	ImGui::Separator();

	// std::vector の全要素に対して処理
	for (int i = 0; i < sprites.size(); ++i)
	{
		if (i != selectedSpriteIndex) {
			continue;
		}
		// IDをプッシュ（これが無いと全部のスプライトが同時に動いてしまう）
		ImGui::PushID(i);
		ImGui::Text("Sprite %d", i);
		//座標
		Vector2 pos = sprites[i]->GetPosition();
		if (ImGui::DragFloat2("Pos", &pos.x, 1.0f)) {
			sprites[i]->SetPosition(pos);
		}//回転
		float rot = sprites[i]->GetRotation();
		if (ImGui::DragFloat("Rot", &rot, 0.01f)) {
			sprites[i]->SetRotation(rot);
		}//サイズ
		Vector2 size = sprites[i]->GetSize();
		if (ImGui::DragFloat2("Size", &size.x, 1.0f)) {
			sprites[i]->SetSize(size);
		}//アンカーポイント
		Vector2 anchor = sprites[i]->GetAnchorPoint();
		// 0.0～1.0 の範囲で動かす
		if (ImGui::DragFloat2("Anchor", &anchor.x, 0.01f, 0.0f, 1.0f)) {
			sprites[i]->SetAnchorPoint(anchor);
		}//左右反転
		bool isFlipX = sprites[i]->GetIsFlipX();
		if (ImGui::Checkbox("isFlipX", &isFlipX)) {
			sprites[i]->SetIsFlipX(isFlipX);
		}
		//上下反転
		bool isFlipY = sprites[i]->GetIsFlipY();
		if (ImGui::Checkbox("isFlipY", &isFlipY)) {
			sprites[i]->SetIsFlipY(isFlipY);
		}//左上座標
		Vector2 texBase = sprites[i]->GetTextureLeftTop();
		if (ImGui::DragFloat2("TexLeftTop", &texBase.x, 1.0f)) {
			sprites[i]->SetTextureLeftTop(texBase);
		}//切り出しサイズ
		Vector2 texSize = sprites[i]->GetTextureSize();
		if (ImGui::DragFloat2("TexSize", &texSize.x, 1.0f)) {
			sprites[i]->SetTextureSize(texSize);
		}
		DrawSpritePlacementCopy(*sprites[i]);

		ImGui::Separator();
		ImGui::PopID();// IDをポップ
	}

	if (!embedded) {
		ImGui::End();
	}
	return selectedSpriteIndex;
#else
	(void)sprites;
	(void)embedded;
	(void)forcedSpriteIndex;
	return -1;
#endif
}

void ImGuiManager::ModelWindow(
	std::vector<std::unique_ptr<Object3d>>& normalObjects,
	std::vector<std::unique_ptr<Object3d>>& animationObjects,
	Object3d::DirectionalLight& light,
	Object3d::PointLight& pointLight,
	Object3d::SpotLight& spotLight,
	bool embedded,
	size_t protectedNormalObjectCount,
	size_t protectedAnimationObjectCount,
	int forcedNormalObjectIndex,
	int forcedAnimationObjectIndex,
	const std::function<void(bool animationObject, size_t index)>& onObjectRemoved)
{
#ifdef USE_IMGUI

	static int selectedNormalIndex = 0;// 0:アニメーションなし 1:アニメーションあり
	static int selectedAnimationIndex = 0;//選択されている番号
	static int previousNormalCount = 0;
	static int previousAnimationCount = 0;
	if (!embedded && !showModelWindow_) {
		return;
	}
	if (!embedded && !ImGui::Begin("Editing Object", &showModelWindow_)) {
		ImGui::End();
		return;
	}

	auto syncSelectionForChangedList = [](int objectCount, size_t protectedObjectCount, int& selectedIndex, int& previousCount) {
		const int protectedCount = static_cast<int>(protectedObjectCount);
		if (objectCount <= 0) {
			selectedIndex = 0;
			previousCount = 0;
			return;
		}
		if (objectCount != previousCount) {
			const bool hasAddedObjects = objectCount > protectedCount;
			if (objectCount > previousCount || selectedIndex >= objectCount || (hasAddedObjects && selectedIndex < protectedCount)) {
				selectedIndex = hasAddedObjects ? objectCount - 1 : 0;
			}
			previousCount = objectCount;
		}
	};

	syncSelectionForChangedList(static_cast<int>(normalObjects.size()), protectedNormalObjectCount, selectedNormalIndex, previousNormalCount);
	syncSelectionForChangedList(static_cast<int>(animationObjects.size()), protectedAnimationObjectCount, selectedAnimationIndex, previousAnimationCount);
	const bool forceNormalSelection =
		forcedNormalObjectIndex >= 0 && forcedNormalObjectIndex < static_cast<int>(normalObjects.size());
	const bool forceAnimationSelection =
		forcedAnimationObjectIndex >= 0 && forcedAnimationObjectIndex < static_cast<int>(animationObjects.size());
	if (forceNormalSelection) {
		selectedNormalIndex = forcedNormalObjectIndex;
	}
	if (forceAnimationSelection) {
		selectedAnimationIndex = forcedAnimationObjectIndex;
	}

	auto drawObjectEditor = [this, &onObjectRemoved](const char* label, std::vector<std::unique_ptr<Object3d>>& objects, int& selectedIndex, size_t protectedObjectCount, bool animationObject) {
		if (objects.empty()) {
			ImGui::TextDisabled("No %s objects.", label);
			return;
		}
		if (selectedIndex < 0 || selectedIndex >= static_cast<int>(objects.size())) {
			selectedIndex = 0;
		}

		auto makeObjectLabel = [label](const Object3d* object, int index) {
			const std::string modelName = object ? object->GetModelName() : std::string();
			if (!modelName.empty()) {
				return std::string(label) + " " + std::to_string(index) + " : " + modelName;
			}
			return std::string(label) + " " + std::to_string(index);
		};

		std::string preview = makeObjectLabel(objects[selectedIndex].get(), selectedIndex);
		if (ImGui::BeginCombo("Target", preview.c_str())) {
			for (int index = 0; index < static_cast<int>(objects.size()); ++index) {
				std::string item = makeObjectLabel(objects[index].get(), index);
				if (ImGui::Selectable(item.c_str(), selectedIndex == index)) {
					selectedIndex = index;
				}
			}
			ImGui::EndCombo();
		}

		Object3d* targetObject = objects[selectedIndex].get();
		ImGui::PushID(targetObject);
		if (!targetObject->GetModelName().empty()) {
			ImGui::Text("Model File: %s", targetObject->GetModelName().c_str());
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Time Playback");
		if (targetObject->IsAnimating()) {
			bool animationReturning = targetObject->IsAnimationReturning();
			if (ImGui::Checkbox("return##Animation", &animationReturning)) {
				targetObject->SetAnimationReturning(animationReturning);
			}
			ImGui::SameLine();
			ImGui::TextDisabled(
				"Animation %.2f / %.2f sec",
				targetObject->GetAnimationTime(),
				targetObject->GetAnimationDuration());
		}

		bool transformReturning = targetObject->IsTransformReturning();
		ImGui::BeginDisabled(!targetObject->HasTransformHistory());
		if (ImGui::Checkbox("return##Transform", &transformReturning)) {
			targetObject->SetTransformReturning(transformReturning);
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!targetObject->CanMoveTransformForward());
		if (ImGui::Button("Move")) {
			targetObject->MoveTransformForward();
		}
		ImGui::EndDisabled();
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (targetObject->IsTransformReturning()) {
			ImGui::TextDisabled("Transform returning %.0f%%", targetObject->GetTransformPlaybackProgress() * 100.0f);
		} else if (targetObject->IsTransformMovingForward()) {
			ImGui::TextDisabled("Transform moving %.0f%%", targetObject->GetTransformPlaybackProgress() * 100.0f);
		} else if (!targetObject->HasTransformHistory()) {
			ImGui::TextDisabled("Edit a transform first");
		}
		if (targetObject->HasTransformHistory()) {
			ImGui::TextDisabled(
				"Transform timeline %.2f / %.2f sec (1x)",
				targetObject->GetTransformPlaybackTime(),
				targetObject->GetTransformPlaybackDuration());
		}

		Transform& transform = targetObject->GetTransform();
		const Transform transformBeforeEdit = transform;
		bool transformEdited = false;
		const bool transformPlaybackActive =
			targetObject->IsTransformReturning() || targetObject->IsTransformMovingForward();
		ImGui::BeginDisabled(transformPlaybackActive);
		transformEdited |= ImGui::DragFloat3("Translate", &transform.translate.x, 0.01f);
		transformEdited |= ImGui::DragFloat3("Rotate", &transform.rotate.x, 0.01f);
		transformEdited |= ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f, 0.01f, 100.0f);

		ImGui::Separator();
		ImGui::TextDisabled("Quick Adjust");
		if (ImGui::Button("Reset Transform")) {
			transform.translate = { 0.0f, 0.0f, 0.0f };
			transform.rotate = { 0.0f, 0.0f, 0.0f };
			transform.scale = { 1.0f, 1.0f, 1.0f };
			transformEdited = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Rotation")) {
			transform.rotate = { 0.0f, 0.0f, 0.0f };
			transformEdited = true;
		}

		float uniformScale = (transform.scale.x + transform.scale.y + transform.scale.z) / 3.0f;
		if (ImGui::DragFloat("Uniform Scale", &uniformScale, 0.01f, 0.001f, 100.0f)) {
			transform.scale = { uniformScale, uniformScale, uniformScale };
			transformEdited = true;
		}
		ImGui::EndDisabled();
		if (transformEdited) {
			targetObject->RecordTransformEdit(transformBeforeEdit);
		}

		float environmentCoefficient = targetObject->GetEnvironmentCoefficient();
		if (ImGui::SliderFloat("Environment Reflection", &environmentCoefficient, 0.0f, 1.0f)) {
			targetObject->SetEnvironmentCoefficient(environmentCoefficient);
		}

		ImGui::Separator();
		const bool canRemoveObject = static_cast<size_t>(selectedIndex) >= protectedObjectCount;
		ImGui::BeginDisabled(!canRemoveObject);
		const bool removeSelected = ImGui::Button("Remove Added Model") ||
			(canRemoveObject && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
				!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false));
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip(canRemoveObject
				? "Remove this model from the scene. Delete key also works while Inspector is focused."
				: "Initial scene models are protected. Only models added from Model Shelf can be removed here.");
		}
		if (removeSelected && canRemoveObject) {
			const size_t removedIndex = static_cast<size_t>(selectedIndex);
			if (dxCommon_) {
				dxCommon_->WaitForGPU();
			}
			objects.erase(objects.begin() + selectedIndex);
			if (onObjectRemoved) {
				onObjectRemoved(animationObject, removedIndex);
			}
			if (selectedIndex >= static_cast<int>(objects.size())) {
				selectedIndex = static_cast<int>(objects.size()) - 1;
			}
			if (selectedIndex < 0) {
				selectedIndex = 0;
			}
		}
		ImGui::PopID();
	};

	if (ImGui::BeginTabBar("ModelInspectorTabs")) {
		const ImGuiTabItemFlags modelTabFlags = forceNormalSelection ? ImGuiTabItemFlags_SetSelected : 0;
		const ImGuiTabItemFlags animationTabFlags = forceAnimationSelection ? ImGuiTabItemFlags_SetSelected : 0;
		if (ImGui::BeginTabItem("Model", nullptr, modelTabFlags)) {
			drawObjectEditor("Model", normalObjects, selectedNormalIndex, protectedNormalObjectCount, false);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Animation Model", nullptr, animationTabFlags)) {
			ImGui::Checkbox("Skeleton Debug", &showSkeletonDebugDraw_);
			drawObjectEditor("Animation", animationObjects, selectedAnimationIndex, protectedAnimationObjectCount, true);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Light")) {
			ImGui::Text("Directional Light");
			ImGui::DragFloat3("DirectoinalLight:direction", &light.direction.x, 0.01f);
			ImGui::DragFloat("DirectoinalLight:intensity", &light.intensity, 0.01f);
			ImGui::DragFloat3("DirectoinalLight:color", &light.color.x, 0.01f);

			ImGui::Separator();

			ImGui::Text("Point Light");
			ImGui::DragFloat3("PointLight:position", &pointLight.position.x, 0.01f);
			ImGui::DragFloat("PointLight:intensity", &pointLight.intensity, 0.01f, 0.0f, 10.0f);
			ImGui::ColorEdit3("PointLight:color", &pointLight.color.x);
			ImGui::DragFloat("PointLight:radius", &pointLight.radius, 0.1f);
			// 下限10・上限省略だと直感的に調整できないため、ほかのLightと同じ範囲にします。
			ImGui::DragFloat("PointLight:decay", &pointLight.decay, 0.1f, 0.1f, 10.0f);

			ImGui::Separator();

			ImGui::Text("Spot Light");
			ImGui::DragFloat3("SpotLight:position", &spotLight.position.x, 0.01f);
			ImGui::DragFloat3("SpotLight:direction", &spotLight.direction.x, 0.01f);
			ImGui::DragFloat("SpotLight:intensity", &spotLight.intensity, 0.01f, 0.0f, 20.0f);
			ImGui::ColorEdit3("SpotLight:color", &spotLight.color.x);
			ImGui::SliderFloat("SpotLight:distance", &spotLight.distance, 0.0f, 100.0f);
			ImGui::SliderFloat("SpotLight:decay", &spotLight.decay, 0.1f, 10.0f);
			ImGui::SliderFloat("SpotLight:cosAngle", &spotLight.cosAngle, -1.0f, 1.0f);
			ImGui::SliderFloat("SpotLight:cosFalloffStart", &spotLight.cosFalloffStart, -1.0f, 1.0f);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	
	if (!embedded) {
		ImGui::End();
	}
#endif
}

std::string ImGuiManager::ParticleWindow(Transform& emitterTransform, bool embedded)
{
#ifdef USE_IMGUI
	std::string result = ""; // 何も押されていなければ空文字を返す

	if (!embedded && !showParticleWindow_) {
		return "";
	}
	if (!embedded && !ImGui::Begin("Editing Particle", &showParticleWindow_)) {
		ImGui::End();
		return result;
	}
	ImGui::Separator();

	if (ImGui::Button("Circle Texture"))
	{
		result = "Circle"; // Circleが押されたと報告
	}
	ImGui::SameLine();

	if (ImGui::Button("Plane Texture"))
	{
		result = "Plane"; // Planeが押されたと報告
	}

	ImGui::Separator();
	ImGui::Text("Emitter Transform");
	ImGui::DragFloat3("Emitter Translate", &emitterTransform.translate.x, 0.01f);
	ImGui::DragFloat3("Emitter Rotate", &emitterTransform.rotate.x, 0.01f);
	ImGui::DragFloat3("Emitter Scale", &emitterTransform.scale.x, 0.01f, 0.01f, 10.0f);

	ImGui::Separator();
	ParticleManager* particleManager = ParticleManager::GetInstance();
	bool particlesReturning = particleManager->IsReturning();
	if (ImGui::Checkbox("return##Particles", &particlesReturning)) {
		particleManager->SetReturning(particlesReturning);
	}
	ImGui::SameLine();
	ImGui::TextDisabled(particlesReturning ? "Particles reverse continuously" : "Particles play forward");

	bool autoWindSwitch = particleManager->IsAutoWindSwitchEnabled();
	if (ImGui::Checkbox("Auto Wind Every 5s", &autoWindSwitch)) {
		particleManager->SetAutoWindSwitchEnabled(autoWindSwitch);
	}
	bool windEnabled = particleManager->IsWindEnabled();
	if (ImGui::Checkbox("Wind Now", &windEnabled)) {
		particleManager->SetWindEnabled(windEnabled);
	}
	ImGui::SameLine();
	if (ImGui::Button("Switch Wind On/Off")) {
		particleManager->ToggleWind();
	}
	Vector3 windAcceleration = particleManager->GetWindAcceleration();
	if (ImGui::DragFloat3("Wind Strength", &windAcceleration.x, 0.01f)) {
		particleManager->SetWindAcceleration(windAcceleration);
	}

	if (!embedded) {
		ImGui::End();
	}
	return result;
#else
	return "";
#endif
}

void ImGuiManager::CameraWindow(CameraManager* cameraManager, bool embedded)
{
#ifdef USE_IMGUI

	if (!embedded && !showCameraWindow_) {
		return;
	}
	if (!embedded && !ImGui::Begin("Camera Control", &showCameraWindow_)) {
		ImGui::End();
		return;
	}
	ImGui::Separator();

	//カメラ
	Camera* activeCamera = cameraManager->GetActiveCamera();
	if (activeCamera)
	{
		//位置
		Vector3 cameraPos = activeCamera->GetTranslate();
		if (ImGui::DragFloat3("CameraTranslate", &cameraPos.x, 0.01f))
		{
			activeCamera->SetTranslate(cameraPos);
		}
		//角度
		Vector3 cameraRot = activeCamera->GetRotate();
		if (ImGui::DragFloat3("CameraRotate", &cameraRot.x, 0.01f))
		{
			activeCamera->SetRotate(cameraRot);
		}
	}

	ImGui::Separator();

	if (ImGui::Button("Use MainCamera"))
	{//メインカメラ
		cameraManager->SetActiveCamera("MainCamera");
	}
	ImGui::SameLine();
	if (ImGui::Button("Use UpCamera"))
	{//上空カメラ
		cameraManager->SetActiveCamera("UpCamera");
	}

	if (!embedded) {
		ImGui::End();
	}
#endif
}


