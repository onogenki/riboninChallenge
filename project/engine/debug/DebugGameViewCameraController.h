#pragma once

class Camera;
class DebugAssetPreview;

// DebugのGame Viewだけで使う、MouseによるCamera操作部品です。
// Cameraの寿命はScene、Previewの寿命はDebugAssetPreviewが持ち、この部品はDrag状態だけを持ちます。
class DebugGameViewCameraController
{
public:
	// Edit View・Preview状態を考慮しながら、Game ViewのCamera移動・回転・拡縮を更新します。
	void Update(Camera* activeCamera, DebugAssetPreview& assetPreview);
	// Scene終了時に、次回のDebug表示へMouse Drag状態を持ち込まないよう初期化します。
	void Reset() { isDragging_ = false; }

private:
	// trueならMouse Buttonを押したままGame ViewのCameraを操作中です。
	bool isDragging_ = false;
};
