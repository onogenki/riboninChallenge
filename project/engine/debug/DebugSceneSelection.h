#pragma once

#include <cstddef>
#include <memory>
#include <vector>

class DebugEntityRegistry;
class Object3d;
class Sprite;

// DebugSceneのInspector・Edit Viewで使う選択状態だけを管理する部品です。
// ObjectとSpriteの寿命は所有せず、Sceneから渡された一覧の番号だけを保持します。
class DebugSceneSelection
{
public:
	// 通常モデルかAnimationモデルかと、その一覧内の番号です。
	struct ObjectSelection
	{
		bool hasSelection = false;
		bool isAnimationObject = false;
		size_t index = 0;
	};

	// Sprite一覧内の選択番号です。
	struct SpriteSelection
	{
		bool hasSelection = false;
		size_t index = 0;
	};

	// 指定3Dモデルを選択し、対応するECS Entityも選択します。範囲外ならfalseです。
	bool SelectObject(
		bool isAnimationObject,
		size_t index,
		const std::vector<std::unique_ptr<Object3d>>& normalObjects,
		const std::vector<std::unique_ptr<Object3d>>& animationObjects,
		DebugEntityRegistry& entityRegistry);
	// 指定Spriteを選択し、対応するECS Entityも選択します。範囲外ならfalseです。
	bool SelectSprite(
		size_t index,
		const std::vector<std::unique_ptr<Sprite>>& sprites,
		DebugEntityRegistry& entityRegistry);
	// 3Dモデルの選択だけを解除します。
	void ClearObjectSelection();
	// Spriteの選択だけを解除します。
	void ClearSpriteSelection();
	// Scene終了時に、3D・2D両方の選択を解除します。
	void ClearAll();
	// Inspectorで追加直後の対象を選び続ける残りフレームを一つ進めます。
	void UpdateInspectorAutoSelection();

	// Inspector・Edit Viewへ現在の3D選択状態を渡します。
	const ObjectSelection& GetObjectSelection() const { return objectSelection_; }
	// Inspector・Edit Viewへ現在のSprite選択状態を渡します。
	const SpriteSelection& GetSpriteSelection() const { return spriteSelection_; }
	// trueなら今フレームもInspectorで3Dモデルを自動選択します。
	bool IsObjectInspectorAutoSelected() const;
	// trueなら今フレームもInspectorでSpriteを自動選択します。
	bool IsSpriteInspectorAutoSelected() const;

private:
	// 3Dモデルの選択状態です。
	ObjectSelection objectSelection_{};
	// Spriteの選択状態です。
	SpriteSelection spriteSelection_{};
	// 追加直後にInspectorの3DモデルTabを選ぶ残りフレームです。
	int objectInspectorAutoSelectFrames_ = 0;
	// 追加直後にInspectorのSprite Tabを選ぶ残りフレームです。
	int spriteInspectorAutoSelectFrames_ = 0;
};
