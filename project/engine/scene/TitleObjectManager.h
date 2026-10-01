#pragma once

#include "MyMath.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class DirectXCommon;
class Object3d;
class Object3dCommon;
class Sprite;
class SpriteCommon;

// Titleのモデル・Spriteと、初期配置を保護する境界を所有するManagerです。
// Scene切替・Camera・Light・描画は担当せず、生成・追加・解放だけをまとめます。
class TitleObjectManager
{
public:
	// 仮画像のコマ数を、ManagerとSceneで同じ値として使います。
	static constexpr std::size_t kTitleFrameCount = 12;

	// 所有するObject3dとSpriteの完全な型を知るcpp側で生成・破棄します。
	TitleObjectManager();
	~TitleObjectManager();

	// 既存の一括初期化経路を残し、モデルとSpriteを順に準備します。
	bool Initialize(Object3dCommon* object3dCommon, SpriteCommon* spriteCommon, DirectXCommon* directXCommon);
	// Titleの最初の画面用に、白背景と仮画像Spriteだけを先に作ります。
	bool InitializeSprites(SpriteCommon* spriteCommon, DirectXCommon* directXCommon);
	// Spriteを一度描いた後で、監視画面の板と視点移動先の仮床を作ります。
	bool InitializeModel(Object3dCommon* object3dCommon);
	// ウィンドウの大きさが変わっても、背景と仮画像を全画面に保ちます。
	void ResizeTitleSprites();
	// Factoryでモデルを作り、Titleの配置規則で通常／Animation一覧へ追加します。
	bool AddModel(Object3dCommon* object3dCommon, const std::string& fileName);
	// Shelfの仮配置です。画像の縦横比を保ち、同じ数値位置へ追加します。
	bool AddTexture(SpriteCommon* spriteCommon, const std::string& textureFilePath);
	// 本番用は位置とサイズを直接指定します。InspectorのPos/Sizeをそのまま渡せます。
	bool AddTexture(SpriteCommon* spriteCommon, const std::string& textureFilePath,
		const Vector2& position, const Vector2& size);
	// Titleの導入Spriteと3D監視画面を、同じ番号の仮画像に切り替えます。
	bool SetTitleFrame(std::size_t frameIndex);
	// 初期配置は残し、Editorから追加した要素だけを解放します。
	void ClearAdded();
	// Scene側でGPU完了を待ってから呼び、Spriteとモデルを全て解放します。
	void Finalize();

	// Editorには変更可能な一覧を貸します。所有権はこのManagerに残ります。
	std::vector<std::unique_ptr<Object3d>>& GetNormalObjects() { return normalObjects_; }
	std::vector<std::unique_ptr<Object3d>>& GetAnimationObjects() { return animationObjects_; }
	std::vector<std::unique_ptr<Sprite>>& GetSprites() { return sprites_; }
	// 初期配置の件数は、Editorの削除対象と追加画像の配置番号に使います。
	size_t GetBaseNormalObjectCount() const { return baseNormalObjectCount_; }
	size_t GetBaseAnimationObjectCount() const { return baseAnimationObjectCount_; }
	size_t GetBaseSpriteCount() const { return baseSpriteCount_; }

private:
	// 通常モデル、骨付きAnimationモデル、2D画像をそれぞれ所有します。
	std::vector<std::unique_ptr<Object3d>> normalObjects_;
	std::vector<std::unique_ptr<Object3d>> animationObjects_;
	std::vector<std::unique_ptr<Sprite>> sprites_;
	// Initialize完了時の件数です。ClearAddedで残す先頭要素数を表します。
	size_t baseNormalObjectCount_ = 0;
	size_t baseAnimationObjectCount_ = 0;
	size_t baseSpriteCount_ = 0;
};
