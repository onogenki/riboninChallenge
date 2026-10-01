#include "DebugEcsInspector.h"

#include "ImGuiManager.h"

// ECSのEntity一覧と、選択中Entityが持つ簡易BOX Colliderを編集するUIを表示します。
void DebugEcsInspector::Draw(const Context& context)
{
#ifdef USE_IMGUI
	if (!context.world || !context.selectedEntity || !ImGui::BeginTabItem("ECS")) {
		return;
	}

	Ecs::World& world = *context.world;
	Ecs::Entity& selectedEntity = *context.selectedEntity;
	ImGui::TextWrapped("Every Debug model and 2D texture is registered as an Entity.");
	ImGui::TextDisabled("Add a Box Collider to a 3D model when you need simple collision checks.");
	ImGui::Separator();

	const std::vector<Ecs::Entity>& entities = world.GetEntities();
	if (entities.empty()) {
		ImGui::TextDisabled("No Entity exists in this scene.");
		ImGui::EndTabItem();
		return;
	}

	ImGui::BeginChild("EcsEntityList", ImVec2(0.0f, 110.0f), true);
	for (Ecs::Entity entity : entities) {
		const Ecs::NameComponent* name = world.GetName(entity);
		const char* type = world.HasModel(entity) ? "3D" : "2D";
		const std::string label =
			"#" + std::to_string(entity) + " [" + type + "] " +
			(name ? name->value : "Unnamed");
		if (ImGui::Selectable(label.c_str(), selectedEntity == entity)) {
			selectedEntity = entity;
		}
	}
	ImGui::EndChild();

	if (!world.IsAlive(selectedEntity)) {
		selectedEntity = entities.front();
	}

	ImGui::Separator();
	const Ecs::NameComponent* name = world.GetName(selectedEntity);
	ImGui::Text("Entity #%u", selectedEntity);
	if (name) {
		ImGui::TextWrapped("Name: %s", name->value.c_str());
	}

	if (const Ecs::TransformComponent* transform = world.GetTransform(selectedEntity)) {
		ImGui::TextDisabled("Transform is synchronized from the selected Object3d.");
		ImGui::Text(
			"Position: %.2f, %.2f, %.2f",
			transform->value.translate.x,
			transform->value.translate.y,
			transform->value.translate.z);
		ImGui::Text(
			"Scale: %.2f, %.2f, %.2f",
			transform->value.scale.x,
			transform->value.scale.y,
			transform->value.scale.z);
	}

	if (!world.HasModel(selectedEntity)) {
		ImGui::TextDisabled("2D textures are entities too. Box Collider is currently available for 3D models only.");
		ImGui::EndTabItem();
		return;
	}

	if (!world.HasBoxCollider(selectedEntity)) {
		if (ImGui::Button("Add Box Collider")) {
			world.AddBoxCollider(selectedEntity);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("Default size: 1 x 1 x 1");
	} else if (Ecs::BoxColliderComponent* collider = world.GetBoxCollider(selectedEntity)) {
		ImGui::Separator();
		ImGui::Text("Box Collider");
		ImGui::Checkbox("Enabled", &collider->enabled);
		ImGui::SameLine();
		ImGui::Checkbox("Trigger", &collider->isTrigger);
		ImGui::DragFloat3("Center Offset", &collider->center.x, 0.01f);
		ImGui::DragFloat3("Half Extents", &collider->halfExtents.x, 0.01f, 0.01f, 100.0f);
		ImGui::TextColored(
			collider->isColliding ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : ImVec4(0.4f, 0.75f, 1.0f, 1.0f),
			collider->isColliding ? "Collision: overlapping" : "Collision: no overlap");
		if (ImGui::Button("Remove Box Collider")) {
			world.RemoveBoxCollider(selectedEntity);
		}
	}

	ImGui::EndTabItem();
#else
	(void)context;
#endif
}
