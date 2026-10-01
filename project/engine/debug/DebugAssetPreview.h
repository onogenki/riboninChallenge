#pragma once

#include "../math/Vector3.h"
#include <memory>
#include <string>

class Camera;
class Input;
class Object3d;
class Sprite;
class SpriteCommon;

// Debug画面のモデル・Textureプレビューと、プレビュー中だけのCamera操作を担当します。
// Resource ShelfやDebugSceneはモデルを生成して渡すだけで、プレビュー状態の寿命はこの部品が所有します。
class DebugAssetPreview
{
public:
	// 作成済みモデルをプレビューへ移し、現在のCamera位置を復帰用として保存します。
	bool EnterModel(
		std::unique_ptr<Object3d> object,
		const std::string& fileName,
		const Vector3& modelCenter,
		float modelRadius,
		Camera* camera);
	// TextureからSpriteを作成し、現在のCamera位置を復帰用として保存します。
	bool EnterTexture(SpriteCommon* spriteCommon, const std::string& textureFilePath, Camera* camera);
	// プレビューを終了し、開始前のCamera位置・回転を戻します。
	void Exit(Camera* camera);
	// Scene終了時に、Cameraへ触れずプレビュー所有物だけを解放します。
	void Clear();
	// モデル用Camera、またはTextureの表示位置・大きさを初期値へ戻します。
	void Reset(Camera* camera);
	// プレビュー中のMouse操作とRキー操作を更新します。プレビュー中ならtrueを返します。
	bool UpdateInput(Camera* camera, Input* input, bool isMouseOverGameView, bool isCameraDragging);
	// 外側をクリックした時に、プレビューを閉じてよい状態かを判定します。
	bool ShouldExitOnOutsideClick(bool isLeftMouseDown, bool isLeftMouseClicked, bool isMouseOverGameView);

	bool IsActive() const { return isActive_; }
	bool IsTexturePreview() const { return isTexturePreview_; }
	Object3d* GetObject() const { return object_.get(); }
	Sprite* GetSprite() const { return sprite_.get(); }
	const std::string& GetDisplayName() const { return displayName_; }
	const std::string& GetTextureFilePath() const { return textureFilePath_; }

private:
	// Textureの縦横比を保ちつつ、Game View内へ収まる大きさと位置へ戻します。
	void ResetTextureView();
	// モデル中心を注視する初期Camera位置・回転へ戻します。
	void ResetModelCamera(Camera* camera);
	// プレビューへ入る直前のCamera状態を、一度だけ保存します。
	void SaveReturnCamera(Camera* camera);

	// プレビュー中だけ所有する3Dモデルです。
	std::unique_ptr<Object3d> object_;
	// Textureプレビュー中だけ所有する2D Spriteです。
	std::unique_ptr<Sprite> sprite_;
	// ShelfとGame Viewへ表示するモデル名またはTextureパスです。
	std::string displayName_;
	// Textureプレビューの有効性を確認するためのTextureパスです。
	std::string textureFilePath_;
	// プレビュー開始前のCamera位置・回転です。
	Vector3 returnCameraTranslate_{};
	Vector3 returnCameraRotate_{};
	// モデルプレビューCameraが注視するワールド座標です。
	Vector3 cameraTarget_{};
	// 注視点からCameraまでの距離です。
	float cameraDistance_ = 3.0f;
	// リセット時に戻すCamera距離です。
	float defaultCameraDistance_ = 3.0f;
	// モデルプレビューCameraの左右・上下角度です。
	float cameraYaw_ = 0.0f;
	float cameraPitch_ = 0.0f;
	// trueならモデルまたはTextureのプレビューを表示中です。
	bool isActive_ = false;
	// trueならobject_ではなくsprite_を表示します。
	bool isTexturePreview_ = false;
	// Shelfをクリックした直後のMouse離しで、すぐ終了しないようにします。
	bool suppressExitUntilMouseRelease_ = false;
};
