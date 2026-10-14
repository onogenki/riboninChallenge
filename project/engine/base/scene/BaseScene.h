#pragma once
#include "Vector3.h"
#include <memory>

// 前方宣言により、全Sceneが不要な描画・Audioヘッダを読み込まないようにします。
class SceneManager;
class CameraManager;
class Camera;

// SceneManagerが扱うScene共通の開始・更新・描画・終了の呼び出し口です。
// Cameraの所有は各Sceneに置き、共通の初期化だけをここで補助します。
class BaseScene
{
public:
	// 共通の状態変数を持たないSceneの基底を生成します。
	BaseScene();
	// 派生Sceneを基底ポインタから破棄できるようにします。
	virtual ~BaseScene();

	// SceneManagerが切替を確定した時に呼び、Scene固有の資源を準備します。
	virtual void Initialize() = 0;
	// SceneManagerがSceneを離れる時に呼び、Scene固有の資源を解放します。
	virtual void Finalize() = 0;
	// 毎フレーム、入力とScene内の状態を描画より先に更新します。
	virtual void Update() = 0;
	// Update済みの状態を描画し、描画先を閉じます。
	virtual void Draw() = 0;
	// 既存の状態照会APIです。現行のSceneManagerは参照せず、切替はChangeSceneで行います。
	virtual bool IsFinished() const { return false; }
	// 遷移先の必須資源がそろうまでは、白いフェードを解除しません。
	virtual bool IsReadyForTransition() const { return true; }

protected:
	// Sceneが所有するCameraへ生成結果を渡し、指定位置・回転で最初の有効Cameraにします。
	void InitializeMainCamera(
		std::unique_ptr<CameraManager>& cameraManager,
		std::unique_ptr<Camera>& mainCamera,
		const Vector3& translate,
		const Vector3& rotate = {});

};

