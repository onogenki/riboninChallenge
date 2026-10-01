#pragma once

#include <cstddef>
#include <memory>
#include <vector>

class Camera;
class DirectXCommon;
class Object3d;
class Object3dCommon;
class Sprite;
class SpriteCommon;
struct Vector4;

// Scene共通の描画開始・PostEffect・ImGui出力の順番をまとめる部品です。
// 各Sceneはこの部品を使い、「何を描くか」だけをDraw関数に書きます。
class SceneRenderPipeline
{
public:
	// 3D一覧のうち、どの種類を描くかを選ぶ条件です。
	enum class ObjectDrawFilter
	{
		// Animationの有無を問わず全Object3dを描きます。
		kAll,
		// Skeletonを持つAnimation Objectだけを描きます。
		kSkeletalOnly,
		// Skeletonを持たない通常Objectだけを描きます。
		kNonSkeletalOnly,
	};

	// RenderTextureへの描画を始める前に、DirectXとSRVの共通設定を行います。
	static void Begin(DirectXCommon* directXCommon, const Vector4* clearColorOverride = nullptr);
	// Object3dの共通Pipelineを設定してから、指定条件に合う一覧をまとめて描画します。
	static void DrawObjects(
		Object3dCommon* object3dCommon,
		const std::vector<std::unique_ptr<Object3d>>& objects,
		ObjectDrawFilter filter = ObjectDrawFilter::kAll);
	// Spriteの共通Pipelineを設定し、指定位置以降の空でない要素だけ描画します。
	static void DrawSprites(
		SpriteCommon* spriteCommon,
		const std::vector<std::unique_ptr<Sprite>>& sprites,
		size_t firstSpriteIndex = 0);
	// Sceneのモデル描画後に、PostEffect・SwapChain・必要ならImGui・Presentまでを順番に実行します。
	static void End(DirectXCommon* directXCommon, const Camera* activeCamera, bool drawImGui = true);
};
