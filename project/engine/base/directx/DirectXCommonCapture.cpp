#include "DirectXCommon.h"

#include <cstring>
#include <tmmintrin.h>

// Game ViewのRenderTextureをCPUメモリへ読み戻す処理をまとめる。
// 描画開始・終了の流れと分けることで、通常のScene描画を追いやすくする。
bool DirectXCommon::CaptureGameTexturePixels(std::vector<unsigned char>& pixels, int& outWidth, int& outHeight, bool usePostEffectTexture)
{
	ID3D12Resource* sourceResource = usePostEffectTexture
		? postEffectTextureResource_.Get()
		: renderTextureResource_.Get();
	if (!sourceResource || !device_ || !commandList_ || !commandQueue) {
		return false;
	}

	const D3D12_RESOURCE_DESC sourceDesc = sourceResource->GetDesc();
	if (sourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		sourceDesc.Width == 0 || sourceDesc.Height == 0 ||
		sourceDesc.DepthOrArraySize != 1 || sourceDesc.MipLevels == 0) {
		return false;
	}

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT numRows = 0;
	UINT64 rowSizeInBytes = 0;
	UINT64 totalBytes = 0;
	device_->GetCopyableFootprints(&sourceDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);
	if (totalBytes == 0 || numRows == 0 || rowSizeInBytes == 0) {
		return false;
	}

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

	D3D12_RESOURCE_DESC readbackDesc{};
	readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	readbackDesc.Width = totalBytes;
	readbackDesc.Height = 1;
	readbackDesc.DepthOrArraySize = 1;
	readbackDesc.MipLevels = 1;
	readbackDesc.SampleDesc.Count = 1;
	readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	Microsoft::WRL::ComPtr<ID3D12Resource> readbackResource;
	HRESULT result = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&readbackDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&readbackResource));
	if (FAILED(result)) {
		return false;
	}

	const bool sourceWasShaderResource = usePostEffectTexture
		? isPostEffectTextureShaderResource_
		: isRenderTextureShaderResource_;
	const D3D12_RESOURCE_STATES sourceStateBefore = sourceWasShaderResource
		? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		: D3D12_RESOURCE_STATE_RENDER_TARGET;

	D3D12_RESOURCE_BARRIER toCopySource{};
	toCopySource.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toCopySource.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	toCopySource.Transition.pResource = sourceResource;
	toCopySource.Transition.StateBefore = sourceStateBefore;
	toCopySource.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	toCopySource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList_->ResourceBarrier(1, &toCopySource);

	D3D12_TEXTURE_COPY_LOCATION copySource{};
	copySource.pResource = sourceResource;
	copySource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	copySource.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION copyDestination{};
	copyDestination.pResource = readbackResource.Get();
	copyDestination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	copyDestination.PlacedFootprint = footprint;

	commandList_->CopyTextureRegion(&copyDestination, 0, 0, 0, &copySource, nullptr);

	D3D12_RESOURCE_BARRIER restoreState = toCopySource;
	restoreState.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	restoreState.Transition.StateAfter = sourceStateBefore;
	commandList_->ResourceBarrier(1, &restoreState);

	result = commandList_->Close();
	if (FAILED(result)) {
		return false;
	}

	ID3D12CommandList* commandLists[] = { commandList_.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);
	WaitForGPU();

	outWidth = static_cast<int>(sourceDesc.Width);
	outHeight = static_cast<int>(sourceDesc.Height);
	const size_t tightImageSize = static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight) * 4;
	pixels.resize(tightImageSize);
	unsigned char* mappedData = nullptr;
	D3D12_RANGE readRange{ 0, totalBytes };
	result = readbackResource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));
	if (FAILED(result) || !mappedData) {
		commandAllocators_[frameIndex_]->Reset();
		commandList_->Reset(commandAllocators_[frameIndex_].Get(), nullptr);
		return false;
	}

	const size_t sourceRowPitch = static_cast<size_t>(footprint.Footprint.RowPitch);
	const size_t tightRowPitch = static_cast<size_t>(outWidth) * 4;
	for (int y = 0; y < outHeight; ++y) {
		const unsigned char* sourceRow = mappedData + footprint.Offset + static_cast<size_t>(y) * sourceRowPitch;
		unsigned char* destinationRow = pixels.data() + static_cast<size_t>(y) * tightRowPitch;
		for (int x = 0; x < outWidth; ++x) {
			const unsigned char* sourcePixel = sourceRow + static_cast<size_t>(x) * 4;
			unsigned char* destinationPixel = destinationRow + static_cast<size_t>(x) * 4;
			destinationPixel[0] = sourcePixel[2];
			destinationPixel[1] = sourcePixel[1];
			destinationPixel[2] = sourcePixel[0];
			destinationPixel[3] = sourcePixel[3];
		}
	}
	D3D12_RANGE writtenRange{ 0, 0 };
	readbackResource->Unmap(0, &writtenRange);

	result = commandAllocators_[frameIndex_]->Reset();
	if (FAILED(result)) {
		return false;
	}
	result = commandList_->Reset(commandAllocators_[frameIndex_].Get(), nullptr);
	return SUCCEEDED(result);
}

bool DirectXCommon::QueueGameTextureCapture(bool usePostEffectTexture)
{
	ID3D12Resource* sourceResource = usePostEffectTexture
		? postEffectTextureResource_.Get()
		: renderTextureResource_.Get();
	if (!sourceResource || !device_ || !commandQueue || !fence) {
		return false;
	}

	CaptureReadbackSlot* slot = nullptr;
	for (CaptureReadbackSlot& candidate : captureReadbackSlots_) {
		if (!candidate.pending) {
			slot = &candidate;
			break;
		}
	}
	if (!slot) {
		return false;
	}

	const D3D12_RESOURCE_DESC sourceDesc = sourceResource->GetDesc();
	if (sourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		sourceDesc.Width == 0 || sourceDesc.Height == 0 ||
		sourceDesc.DepthOrArraySize != 1 || sourceDesc.MipLevels == 0) {
		return false;
	}

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT numRows = 0;
	UINT64 rowSizeInBytes = 0;
	UINT64 totalBytes = 0;
	device_->GetCopyableFootprints(
		&sourceDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);
	if (totalBytes == 0 || numRows == 0 || rowSizeInBytes == 0) {
		return false;
	}

	if (!slot->readbackResource || slot->totalBytes != totalBytes) {
		D3D12_HEAP_PROPERTIES heapProperties{};
		heapProperties.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC readbackDesc{};
		readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		readbackDesc.Width = totalBytes;
		readbackDesc.Height = 1;
		readbackDesc.DepthOrArraySize = 1;
		readbackDesc.MipLevels = 1;
		readbackDesc.SampleDesc.Count = 1;
		readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		slot->readbackResource.Reset();
		const HRESULT createResult = device_->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&readbackDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&slot->readbackResource));
		if (FAILED(createResult)) {
			return false;
		}
	}

	HRESULT result = S_OK;
	if (!slot->commandAllocator) {
		result = device_->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&slot->commandAllocator));
		if (FAILED(result)) {
			return false;
		}
	}
	if (!slot->commandList) {
		result = device_->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			slot->commandAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&slot->commandList));
	} else {
		result = slot->commandAllocator->Reset();
		if (SUCCEEDED(result)) {
			result = slot->commandList->Reset(slot->commandAllocator.Get(), nullptr);
		}
	}
	if (FAILED(result)) {
		return false;
	}

	const bool sourceWasShaderResource = usePostEffectTexture
		? isPostEffectTextureShaderResource_
		: isRenderTextureShaderResource_;
	const D3D12_RESOURCE_STATES sourceStateBefore = sourceWasShaderResource
		? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		: D3D12_RESOURCE_STATE_RENDER_TARGET;

	D3D12_RESOURCE_BARRIER toCopySource{};
	toCopySource.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toCopySource.Transition.pResource = sourceResource;
	toCopySource.Transition.StateBefore = sourceStateBefore;
	toCopySource.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
	toCopySource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	slot->commandList->ResourceBarrier(1, &toCopySource);

	D3D12_TEXTURE_COPY_LOCATION copySource{};
	copySource.pResource = sourceResource;
	copySource.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	copySource.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION copyDestination{};
	copyDestination.pResource = slot->readbackResource.Get();
	copyDestination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	copyDestination.PlacedFootprint = footprint;
	slot->commandList->CopyTextureRegion(&copyDestination, 0, 0, 0, &copySource, nullptr);

	D3D12_RESOURCE_BARRIER restoreState = toCopySource;
	restoreState.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	restoreState.Transition.StateAfter = sourceStateBefore;
	slot->commandList->ResourceBarrier(1, &restoreState);

	result = slot->commandList->Close();
	if (FAILED(result)) {
		return false;
	}
	ID3D12CommandList* commandLists[] = { slot->commandList.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);
	const UINT64 signalValue = ++fenceVal;
	result = commandQueue->Signal(fence.Get(), signalValue);
	if (FAILED(result)) {
		return false;
	}

	slot->footprint = footprint;
	slot->totalBytes = totalBytes;
	slot->fenceValue = signalValue;
	slot->sequence = ++captureSequence_;
	slot->width = static_cast<int>(sourceDesc.Width);
	slot->height = static_cast<int>(sourceDesc.Height);
	slot->pending = true;
	return true;
}

bool DirectXCommon::TryGetGameTextureCapturePixels(
	std::vector<unsigned char>& pixels,
	int& outWidth,
	int& outHeight)
{
	CaptureReadbackSlot* oldestSlot = nullptr;
	for (CaptureReadbackSlot& candidate : captureReadbackSlots_) {
		if (candidate.pending && (!oldestSlot || candidate.sequence < oldestSlot->sequence)) {
			oldestSlot = &candidate;
		}
	}
	if (!oldestSlot || !fence || fence->GetCompletedValue() < oldestSlot->fenceValue) {
		return false;
	}

	outWidth = oldestSlot->width;
	outHeight = oldestSlot->height;
	const size_t tightImageSize =
		static_cast<size_t>(outWidth) * static_cast<size_t>(outHeight) * 4;
	pixels.resize(tightImageSize);

	unsigned char* mappedData = nullptr;
	D3D12_RANGE readRange{ 0, oldestSlot->totalBytes };
	const HRESULT mapResult = oldestSlot->readbackResource->Map(
		0, &readRange, reinterpret_cast<void**>(&mappedData));
	if (FAILED(mapResult) || !mappedData) {
		pixels.clear();
		outWidth = 0;
		outHeight = 0;
		oldestSlot->pending = false;
		return true;
	}

	const size_t sourceRowPitch =
		static_cast<size_t>(oldestSlot->footprint.Footprint.RowPitch);
	const size_t tightRowPitch = static_cast<size_t>(outWidth) * 4;
	const __m128i bgraShuffle = _mm_setr_epi8(
		2, 1, 0, 3,
		6, 5, 4, 7,
		10, 9, 8, 11,
		14, 13, 12, 15);
	const __m128i opaqueAlpha = _mm_setr_epi8(
		0, 0, 0, static_cast<char>(0xFF),
		0, 0, 0, static_cast<char>(0xFF),
		0, 0, 0, static_cast<char>(0xFF),
		0, 0, 0, static_cast<char>(0xFF));
	for (int y = 0; y < outHeight; ++y) {
		const unsigned char* sourceRow =
			mappedData + oldestSlot->footprint.Offset + static_cast<size_t>(y) * sourceRowPitch;
		unsigned char* destinationRow = pixels.data() + static_cast<size_t>(y) * tightRowPitch;
		int x = 0;
		for (; x + 4 <= outWidth; x += 4) {
			const __m128i rgba = _mm_loadu_si128(reinterpret_cast<const __m128i*>(
				sourceRow + static_cast<size_t>(x) * 4));
			const __m128i bgra = _mm_or_si128(_mm_shuffle_epi8(rgba, bgraShuffle), opaqueAlpha);
			_mm_storeu_si128(reinterpret_cast<__m128i*>(
				destinationRow + static_cast<size_t>(x) * 4), bgra);
		}
		for (; x < outWidth; ++x) {
			uint32_t sourcePixel = 0;
			std::memcpy(&sourcePixel, sourceRow + static_cast<size_t>(x) * 4, sizeof(sourcePixel));
			const uint32_t destinationPixel =
				0xFF000000u |
				((sourcePixel & 0x000000FFu) << 16) |
				(sourcePixel & 0x0000FF00u) |
				((sourcePixel & 0x00FF0000u) >> 16);
			std::memcpy(destinationRow + static_cast<size_t>(x) * 4, &destinationPixel, sizeof(destinationPixel));
		}
	}
	D3D12_RANGE writtenRange{ 0, 0 };
	oldestSlot->readbackResource->Unmap(0, &writtenRange);
	oldestSlot->pending = false;
	return true;
}

