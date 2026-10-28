#include "SrvManager.h"
#include <stdexcept>

// DirectXから共通Heapを作り、使用済み番号を残さず全枠を割当可能にします。
void SrvManager::Initialize(DirectXCommon* dxCommon){
	//引数で受け取ってメンバ変数に記録する
	directXCommon_ = dxCommon;

	//デスクリプタヒープの作成
	descriptorHeap_ = directXCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	// 空Heapのまま番号だけ配るとTexture登録で失敗するため、初期化をここで止めます。
	if (!descriptorHeap_) {
		throw std::runtime_error("SrvManager: descriptor heap creation failed.");
	}
	//デスクリプタ1個分のサイズを取得して記録
	descriptorSize_ = directXCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	freeList_ = {};
	allocated_.fill(false);

	//初期化時に番号すべて空きリストに入れておく
	for (uint32_t i = 0; i < kMaxSRVCount; ++i)
	{
		freeList_.push(i) ;
	}

}

bool SrvManager::CanAllocate() const
{
	return !freeList_.empty();
}

bool SrvManager::CanAllocate(uint32_t count) const
{
	return freeList_.size() >= count;
}

uint32_t SrvManager::Allocate()
{
	//上限に達してないかチェック
	if (!CanAllocate()) {
		return kInvalidSrvIndex;
	}
	//キューの先頭から空いてる番号を1つ取り出す
	uint32_t index = freeList_.front();
	//取り出した番号をキューから排除
	freeList_.pop();
	allocated_[index] = true;
	//上で記録した番号をreturn
	return index;
}

void SrvManager::Free(uint32_t index)
{
	//使わなくなった番号を空きリストに戻す
	if (index >= kMaxSRVCount || !allocated_[index]) {
		return;
	}
	allocated_[index] = false;
	freeList_.push(index);
}

// Heap先頭からの距離を番号へ戻し、このHeapに属する枠だけを再利用します。
void SrvManager::Free(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	if (!descriptorHeap_ || descriptorSize_ == 0) {
		return;
	}
	const SIZE_T start = descriptorHeap_->GetCPUDescriptorHandleForHeapStart().ptr;
	if (handle.ptr < start) {
		return;
	}
	const SIZE_T offset = handle.ptr - start;
	if (offset % descriptorSize_ != 0) {
		return;
	}
	Free(static_cast<uint32_t>(offset / descriptorSize_));
}

// 登録先の枠を指定するため、先頭Handleへ一枠の幅と番号を加えます。
D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize_ * index);

	return handleCPU;
}

// Shaderが同じ枠を参照できるよう、GPU側の先頭Handleから番号を計算します。
D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize_ * index);

	return handleGPU;
}

// Textureの形式とMip数をDescriptorへ登録し、描画時の画像参照を可能にします。
void SrvManager::CreateSRVforTexture2D(
	uint32_t srvIndex,
	ID3D12Resource* pResource,
	DXGI_FORMAT Format,
	UINT MipLevels,
	bool forceOpaqueAlpha)
{

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = Format;
	// Game Viewでは最終画像を不透明表示し、パーティクル面の境界が見えないようにする。
	srvDesc.Shader4ComponentMapping = forceOpaqueAlpha
		? D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
			D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
			D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1,
			D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2,
			D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1)
		: D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = MipLevels;

	directXCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));

}

// 構造化Bufferの要素数と幅をSRVへ記録し、Shader側の配列参照を可能にします。
void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	directXCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

// Shaderから書き込む構造化Bufferを、指定番号のUAVへ登録します。
void SrvManager::CreateUAVforStructuredBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = numElements;
	uavDesc.Buffer.StructureByteStride = structureByteStride;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	directXCommon_->GetDevice()->CreateUnorderedAccessView(pResource, nullptr, &uavDesc, GetCPUDescriptorHandle(uavIndex));
}

// Drawより先に共有HeapをCommand Listへ結合し、SRV/UAVの番号を有効にします。
void SrvManager::PreDraw()
{
	//描画用のDescriptorHeapの設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };
	directXCommon_->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

// Graphics Shaderの指定Root Parameterへ、登録済みGPU Handleを渡します。
void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
{
	directXCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

// Compute Shaderの指定Root Parameterへ、同じHeapのGPU Handleを渡します。
void SrvManager::SetComputeRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex)
{
	directXCommon_->GetCommandList()->SetComputeRootDescriptorTable(RootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}
