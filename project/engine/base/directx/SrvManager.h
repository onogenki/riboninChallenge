#pragma once
#include "DirectXCommon.h"
#include <cassert>
#include <array>
#include <cstdint>
#include<queue>

class SrvManager
{
public:
	static constexpr uint32_t kInvalidSrvIndex = UINT32_MAX;

	// シングルトンインスタンスの取得
	static SrvManager* GetInstance() {
		static SrvManager instance;
		return &instance;
	}

	//初期化
	void Initialize(DirectXCommon* dxCommon);

	uint32_t Allocate();
	void Free(uint32_t index);//解放用の関数
	void Free(D3D12_CPU_DESCRIPTOR_HANDLE handle);
	bool CanAllocate() const;//空きがあるか確認する関数

	bool CanAllocate(uint32_t count) const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	//SRV生成(テクスチャ用)
	void CreateSRVforTexture2D(
		uint32_t srvIndex,
		ID3D12Resource* pResource,
		DXGI_FORMAT Format,
		UINT MipLevels,
		bool forceOpaqueAlpha = false);
	//SRV生成(Structured Buffer用)
	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
	void CreateUAVforStructuredBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	void PreDraw();

	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);
	void SetComputeRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

	//デスクリプタヒープを取得
	ID3D12DescriptorHeap* GetDescriptorHeap() { return descriptorHeap.Get(); }

private:

	//シングルトン
	SrvManager() = default;
	~SrvManager() = default;
	SrvManager(const SrvManager&) = delete;
	SrvManager& operator=(const SrvManager&) = delete;


	DirectXCommon* directXCommon = nullptr;

	//最大SRV数(最大テクスチャ枚数)
	static constexpr uint32_t kMaxSRVCount = 512;

	//SRV用のデスクリプタサイズ
	uint32_t descriptorSize = 0;

	//SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	//空き番号を管理するキュー
	std::queue<uint32_t> freeList_;
	std::array<bool, kMaxSRVCount> allocated_{};

};

