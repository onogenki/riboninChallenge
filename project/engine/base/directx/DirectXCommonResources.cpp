#include "DirectXCommon.h"

#include "Logger.h"
#include "StringUtility.h"
#include "SrvManager.h"
#include "externals/DirectXTex/d3dx12.h"

#include <cassert>
#include <cstring>
#include <vector>

using namespace Logger;

// Shaderコンパイル、Descriptor、Texture・Buffer Resource作成と転送をまとめる。
// Device初期化やフレーム描画パスから分離し、GPU Resource操作だけを追えるようにする。
D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVCPUDescriptorHandle(uint32_t index)
{
	return GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetSRVGPUDescriptorHandle(uint32_t index)
{
	return GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, index);
}

Microsoft::WRL::ComPtr < IDxcBlob> DirectXCommon::CompileShader(
	//CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	//Compilerに使用するProfile
	const wchar_t* profile
)
{
	//これからシェーダーをコンバイルする旨をログに出す
	Log(StringUtility::ConvertString(std::format(L"Begin CompileShader,path:{},profile:{}\n", filePath, profile)));
	//hlslファイルを読む
	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	//読めなかったら止める
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;//UTF8の文字コードであることを通知

	LPCWSTR arguments[] = {
		filePath.c_str(),//コンバイル対象のh|s|ファイル名
		L"-E",L"main",//エントリーポイントの指定。基本的にmain以外にはしない
		L"-T",profile,//shaderProfileの設定
		L"-Zi",L"-Qembed_debug",//デバック用の情報を埋め込む
		L"-Od",//最適解を外しておく
		L"-Zpr",//メモリレイアウトは行優先
	};
	Microsoft::WRL::ComPtr < IDxcResult> shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,//読み込んだファイル
		arguments,//コンバイルオプション
		_countof(arguments),//コンバイルオプションの数
		includeHandler.Get(),//includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult)//コンパイル結果
	);
	//コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	//警告・エラーが出てたらログに出して止める
	Microsoft::WRL::ComPtr < IDxcBlobUtf8> shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0)
	{
		Log(shaderError->GetStringPointer());
		//警告・エラーダメ絶対
		assert(false);
	}
	//コンパイル結果から実行用のバイナリ部分を取得
	Microsoft::WRL::ComPtr < IDxcBlob> shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	//成功したログを出す
	Log(StringUtility::ConvertString(std::format(L"Compile,Succeeded,path:{},profile:{}\n", filePath, profile)));
	//実行用のバイナリを返却
	return shaderBlob;
}


Microsoft::WRL::ComPtr < ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	UINT numDescriptors,
	bool shaderVisible)
{
	Microsoft::WRL::ComPtr < ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	
	HRESULT hr = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));

	return descriptorHeap;

}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(Microsoft::WRL::ComPtr < ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(Microsoft::WRL::ComPtr < ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

const uint32_t DirectXCommon::kMaxSRVCount = 512;

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata)
{
	// 1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width); // Textureの幅
	resourceDesc.Height = UINT(metadata.height); // Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元など。基本2D。

	// 2. 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 細かい設定を行う

	// 3. Resourceを生成する
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST, // データ転送される前提の初期状態
		nullptr, // Clear最適化設定。使わないのでnullptr。
		IID_PPV_ARGS(&resource)); // 作成するResourceへのポインタ
	assert(SUCCEEDED(hr));

	return resource;
}


Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateRenderTextureResource(
	uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor)
{
	// RenderTextureとして使う2Dテクスチャの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;// テクスチャの幅
	resourceDesc.Height = height;// テクスチャの高さ
	resourceDesc.MipLevels = 1;// ミップマップは使用しない
	resourceDesc.DepthOrArraySize = 1;// 2Dテクスチャ1枚分
	resourceDesc.Format = format;// RTVとSRVでも同じFormatを使う
	resourceDesc.SampleDesc.Count = 1;// マルチサンプリングは使用しない
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;// 2Dテクスチャとして生成する
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;// RTVとして書き込み可能にする

	// GPU上のVRAMにリソースを作るため、DEFAULT Heapを使用する
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// ClearRenderTargetViewで頻繁に使う色を最適化用のClearValueとして指定する
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = format;
	clearValue.Color[0] = clearColor.x;
	clearValue.Color[1] = clearColor.y;
	clearValue.Color[2] = clearColor.z;
	clearValue.Color[3] = clearColor.w;

	// RenderTargetとして使用することを前提に、初期状態をRENDER_TARGETにして生成する
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,// 使用するHeapの設定
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,// RenderTextureの設定
		D3D12_RESOURCE_STATE_RENDER_TARGET,// 生成直後から描画先として使う
		&clearValue,// 最適化するクリア色
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	// 作成したRenderTextureを呼び出し元へ返す
	return resource;
}

void DirectXCommon::CreateRenderTextureSRV(SrvManager* srvManager)
{
	assert(srvManager);
	assert(renderTextureResource_);
	assert(postEffectTextureResource_);
	assert(gaussianBlurTextureResource_);
	assert(depthStencilResource);

	// RenderTextureの描画結果をShaderから読めるよう、SRVの場所を1個確保する
	if (renderTextureSrvIndex_ == UINT32_MAX) {
		renderTextureSrvIndex_ = srvManager->Allocate();
	}
	if (postEffectTextureSrvIndex_ == UINT32_MAX) {
		postEffectTextureSrvIndex_ = srvManager->Allocate();
	}
	if (gaussianBlurTextureSrvIndex_ == UINT32_MAX) {
		gaussianBlurTextureSrvIndex_ = srvManager->Allocate();
	}
	if (depthStencilSrvIndex_ == UINT32_MAX) {
		depthStencilSrvIndex_ = srvManager->Allocate();
	}

	// Resourceと同じFormat、ミップレベル1でTexture2D用SRVを生成する
	srvManager->CreateSRVforTexture2D(
		renderTextureSrvIndex_,
		renderTextureResource_.Get(),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		1,
		true);

	srvManager->CreateSRVforTexture2D(
		postEffectTextureSrvIndex_,
		postEffectTextureResource_.Get(),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		1,
		true);

	srvManager->CreateSRVforTexture2D(
		gaussianBlurTextureSrvIndex_,
		gaussianBlurTextureResource_.Get(),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		1,
		true);

	D3D12_SHADER_RESOURCE_VIEW_DESC depthStencilSrvDesc{};
	depthStencilSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	depthStencilSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	depthStencilSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	depthStencilSrvDesc.Texture2D.MipLevels = 1;
	device_->CreateShaderResourceView(
		depthStencilResource.Get(),
		&depthStencilSrvDesc,
		srvManager->GetCPUDescriptorHandle(depthStencilSrvIndex_));
}

//CPUのMap/memcpy
[[nodiscard]]
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::WriteToIntermediateResource(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device)
{
	//テクスチャの情報を取得
	D3D12_RESOURCE_DESC desc = texture->GetDesc();

	//ミップマップの数だけ配列を用意する
	const size_t numSubresources = mipImages.GetImageCount();
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>footprints(numSubresources);
	std::vector<UINT> numRows(numSubresources);
	std::vector<UINT64>rowSizeInBytes(numSubresources);
	UINT64 totalBytes = 0;

	// DirectX12に配置図(Footprint)を計算してもらう
	device->GetCopyableFootprints(
		&desc,//テクスチャの設定
		0,//ミップレベル0から1つ分
		UINT(numSubresources),//結果の配置図がここに入る
		0,
		footprints.data(),
		numRows.data(),//行数(高さ)
		rowSizeInBytes.data(),//実際の1桁のデータサイズ(パディング抜き)
		&totalBytes//必要な中間バッファのサイズ
	);

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(totalBytes);

	uint8_t* mappedData = nullptr;
	intermediateResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));

	const DirectX::Image* images = mipImages.GetImages();
	for (size_t i = 0; i < numSubresources; ++i)
	{
		const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint = footprints[i];
		//書き込み先のスタート位置
		uint8_t* dest = mappedData + footprint.Offset;
		//読み込み元の画像データ
		const uint8_t* src = images[i].pixels;

		//1行ずつ、256バイトの隙間ルール(RowPitch)を考慮してコピー
		for (UINT y = 0; y < numRows[i]; ++y)
		{
			uint8_t* destRow = dest + (y * footprint.Footprint.RowPitch);
			const uint8_t* srcRow = src + (y * images[i].rowPitch);

			std::memcpy(destRow, srcRow, rowSizeInBytes[i]);
		}
	}

	//CPUでの書き込みが終わったのでここでUnmap
	intermediateResource->Unmap(0, nullptr);

	//作成してデータを入れた中間リソースを消さないように外へ返す
	return intermediateResource;
}


//GPUのCopyTextureRegion
void DirectXCommon::RecordTextureCopyCommand(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const Microsoft::WRL::ComPtr<ID3D12Resource>& intermediateResource,
	size_t numSubresources,ID3D12Device* device,ID3D12GraphicsCommandList* commandList)
{
	// footprints配置図はここでもう一度計算すれば前の関数から引き回さなくて済む
	D3D12_RESOURCE_DESC desc = texture->GetDesc();
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>footprints(numSubresources);
	// DirectX12にすべてのサブリソース（ミップマップ・配列面）の配置図を計算してもらう
	device->GetCopyableFootprints(
		&desc,//テクスチャの設定
		0,//ミップレベル0から1つ分
		UINT(numSubresources),//結果の配置図がここに入る
		0,
		footprints.data(),
		nullptr,
		nullptr,
		nullptr
	);

	// 全てのサブリソースをループで転送
	for (size_t i = 0; i < numSubresources; ++i)
	{
		// コピー先（VRAMテクスチャ）
		D3D12_TEXTURE_COPY_LOCATION dst{};
		dst.pResource = texture.Get();
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = UINT(i);

		// コピー元（中間バッファの特定の位置）
		D3D12_TEXTURE_COPY_LOCATION src{};
		src.pResource = intermediateResource.Get();
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint = footprints[i];

		// ここでコマンドリストに転送命令を積む
		commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	}

	// Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCEへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	
	commandList->ResourceBarrier(1, &barrier);
}


Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う

	// 頂点リソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際にリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}

void DirectXCommon::ExecuteTextureTransfer(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages)
{

	// メインの commandList_ を汚さないよう、この関数内だけで使う一時的なコマンドリストを作る
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> tempAllocator = nullptr;
	HRESULT hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&tempAllocator));
	assert(SUCCEEDED(hr));

	// 新しく作った tempAllocator を使って、一時的なコマンドリストを作成する
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> tempCommandList = nullptr;
	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, tempAllocator.Get(), nullptr, IID_PPV_ARGS(&tempCommandList));
	assert(SUCCEEDED(hr));

	// CPUの処理：中間バッファにデータを書き込んで、そのポインタを受け取る
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = WriteToIntermediateResource(texture, mipImages, device_.Get());

	// 転送コマンドを積む対象を、新しく作った「tempCommandList.Get()」にする
	RecordTextureCopyCommand(texture, intermediateResource, mipImages.GetImageCount(), device_.Get(), tempCommandList.Get());

	// 一時的なコマンドリストをCloseする
	hr = tempCommandList->Close();
	assert(SUCCEEDED(hr));

	// 実行するコマンドリストを「tempCommandList.Get()」にする
	ID3D12CommandList* commandLists[] = { tempCommandList.Get() };
	commandQueue->ExecuteCommandLists(1, commandLists);

	// GPUの実行完了を安全に待つ
	WaitForGPU();

}

