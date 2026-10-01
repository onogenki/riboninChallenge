#pragma once

#include <string>

// DebugSceneのEdit Viewへ、Preview操作やDrop操作の説明を重ねるUI部品です。
// Previewの開始・終了やモデル生成は扱わず、現在の表示状態だけを受け取ります。
class DebugEditOverlay
{
public:
	struct Context
	{
		bool isPreviewActive = false;
		bool isTexturePreview = false;
		std::string previewDisplayName;
	};

	// Edit Viewの上へ、PreviewまたはDrag & Dropの案内を描画します。
	static void Draw(const Context& context);
};
