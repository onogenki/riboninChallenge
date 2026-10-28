#pragma once
#include "DirectXCommon.h"

namespace Engine
{

// ModelManagerが所有し、各Modelへ共通のDirectX操作窓口を貸します。
// モデルの頂点・材質・Texture自体は所有しません。
class ModelCommon
{
public:
	// Modelを作る前に、ModelManagerからDirectX操作窓口を受け取ります。
	void Initialize(DirectXCommon* dxCommon);
	// ModelのGPU資源生成に使う、所有権のないDirectX操作窓口を返します。
	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
	DirectXCommon* dxCommon_ = nullptr;
};

}

