#pragma once

#include "../ecs/EcsWorld.h"

// DebugSceneが所有するECSの一覧・Box Collider編集UIだけを表示するDebug用部品です。
// EntityやObject3dの寿命は持たず、選択中Entityの番号だけを呼び出し元と共有します。
class DebugEcsInspector
{
public:
	struct Context
	{
		// 表示・編集するECS本体です。所有しません。
		Ecs::World* world = nullptr;
		// Inspectorで現在選択しているEntity番号です。所有しません。
		Ecs::Entity* selectedEntity = nullptr;
	};

	// ECSタブを表示し、Entity選択と簡易BOX Colliderの編集を行います。
	static void Draw(const Context& context);
};
