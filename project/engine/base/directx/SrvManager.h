#pragma once
#include "DirectXCommon.h"
#include <cassert>
#include <array>
#include <cstdint>
#include<queue>

namespace Engine
{
// SRV・UAV用ディスクリプタ番号を割り当て、解放後の番号を再利用します。
class SrvManager
{
public:
	static constexpr uint32_t kInvalidSrvIndex = UINT32_MAX;

	// シングルトンインスタンスの取得
	static SrvManager* GetInstance() {
		static SrvManager instance;
		return &instance;
	}

	// @brief DirectX初期化後に共有Descriptor Heapを作り、全番号を割当可能にします。
	// @param dxCommon 初期化済みのDirectX操作窓口です。このクラスは所有しません。
	// Device未準備やHeap作成失敗では例外を投げ、未完成のHeapを使いません。
	void Initialize(Engine::DirectXCommon* dxCommon);

	uint32_t Allocate();
	void Free(uint32_t index);//解放用の関数
	// このHeap内の枠に一致するHandleだけを解放し、範囲外や未割当なら何もしません。
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

	// 描画と資源登録で同じHeapを使えるよう、所有権を渡さず本体を返します。
	ID3D12DescriptorHeap* GetDescriptorHeap() { return descriptorHeap_.Get(); }

private:

	//シングルトン
	SrvManager() = default;
	~SrvManager() = default;
	SrvManager(const SrvManager&) = delete;
	SrvManager& operator=(const SrvManager&) = delete;


	Engine::DirectXCommon* directXCommon_ = nullptr;

	//最大SRV数(最大テクスチャ枚数)
	static constexpr uint32_t kMaxSRVCount = 512;

	//SRV用のデスクリプタサイズ
	uint32_t descriptorSize_ = 0;

	//SRV用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

	//空き番号を管理するキュー
	std::queue<uint32_t> freeList_;
	std::array<bool, kMaxSRVCount> allocated_{};

};
} // namespace Engine

