#include "PlanarReflectionTarget.h"

#include "DirectXCommon.h"
#include "SrvManager.h"
#include <algorithm>

PlanarReflectionTarget::~PlanarReflectionTarget()
{
	if (srvManager_ && srvIndex_ != UINT32_MAX) {
		srvManager_->Free(srvIndex_);
	}
}

bool PlanarReflectionTarget::Initialize(
	DirectXCommon* dxCommon,
	SrvManager* srvManager,
	uint32_t width,
	uint32_t height)
{
	if (!dxCommon || !srvManager || !srvManager->CanAllocate()) {
		return false;
	}

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	width_ = (std::max)(width, 64u);
	height_ = (std::max)(height, 64u);
	textureResource_ = dxCommon_->CreateRenderTextureResource(
		width_,
		height_,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		clearColor_);
	if (!textureResource_) {
		return false;
	}

	rtvHeap_ = dxCommon_->CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		1,
		false);
	if (!rtvHeap_) {
		textureResource_.Reset();
		return false;
	}
	rtvHandle_ = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	dxCommon_->GetDevice()->CreateRenderTargetView(
		textureResource_.Get(),
		&rtvDesc,
		rtvHandle_);

	srvIndex_ = srvManager_->Allocate();
	if (srvIndex_ == UINT32_MAX) {
		textureResource_.Reset();
		rtvHeap_.Reset();
		return false;
	}
	srvManager_->CreateSRVforTexture2D(
		srvIndex_,
		textureResource_.Get(),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		1,
		true);
	return true;
}

void PlanarReflectionTarget::Begin(D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle)
{
	if (!dxCommon_ || !textureResource_) {
		return;
	}

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	if (isShaderResource_) {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = textureResource_.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
		isShaderResource_ = false;
	}

	commandList->OMSetRenderTargets(1, &rtvHandle_, false, &depthStencilHandle);
	const float clearColor[]{ clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w };
	commandList->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(
		depthStencilHandle,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
		nullptr);

	D3D12_VIEWPORT viewport{};
	viewport.Width = static_cast<float>(width_);
	viewport.Height = static_cast<float>(height_);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	const D3D12_RECT scissor{ 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_) };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
}

void PlanarReflectionTarget::End()
{
	if (!dxCommon_ || !textureResource_ || isShaderResource_) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = textureResource_.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	isShaderResource_ = true;
}
