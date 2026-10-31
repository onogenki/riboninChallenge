#pragma once

#include "Vector4.h"
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

namespace Engine { class DirectXCommon; }
namespace Engine { class SrvManager; }

// 反射Cameraの描画結果を保存し、鏡面Shaderから読めるようにする描画先です。
class PlanarReflectionTarget
{
public:
	~PlanarReflectionTarget();

	bool Initialize(
		Engine::DirectXCommon* dxCommon,
		Engine::SrvManager* srvManager,
		uint32_t width = 512,
		uint32_t height = 512);
	void Begin(D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle);
	void End();

	uint32_t GetSrvIndex() const { return srvIndex_; }
	uint32_t GetWidth() const { return width_; }
	uint32_t GetHeight() const { return height_; }
	bool IsInitialized() const { return textureResource_ != nullptr; }

private:
	Engine::DirectXCommon* dxCommon_ = nullptr;
	Engine::SrvManager* srvManager_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
	uint32_t srvIndex_ = UINT32_MAX;
	uint32_t width_ = 512;
	uint32_t height_ = 512;
	bool isShaderResource_ = false;
	Vector4 clearColor_{ 0.06f, 0.08f, 0.12f, 1.0f };
};
