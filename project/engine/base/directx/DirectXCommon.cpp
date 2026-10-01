#include "DirectXCommon.h"
#include"Logger.h"
#include"StringUtility.h"
#include "SrvManager.h"
#include<algorithm>
#include<cassert>
#include <cstdint>
#include <thread>
using namespace Logger;

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
using namespace Microsoft::WRL;

void DirectXCommon::Initialize(WinApp* winApp)
{
	//NULL検出
	assert(winApp);

	//メンバ変数に記録
	this->winApp = winApp;
	width = winApp->GetClientWidth();
	height = winApp->GetClientHeight();

	//FPS固定初期化
	InitializeFixFPS();

	// 描画に必要なGPU資源を依存順に作成する。
	CreateDevice();
	CreateCommandObjects();
	CreateSwapChain();
	CreateDepthStencilResource();
	CreateDescriptorHeaps();
	CreateRenderTargetViews();
	CreateDepthStencilView();
	CreateFence();
	ConfigureViewport();
	ConfigureScissorRect();
	CreateDxcCompiler();
}

void DirectXCommon::ResizeIfNeeded()
{
	const uint32_t newWidth = winApp->GetClientWidth();
	const uint32_t newHeight = winApp->GetClientHeight();
	if (newWidth == 0 || newHeight == 0 || (newWidth == width && newHeight == height)) {
		return;
	}

	WaitForGPU();
	for (auto& swapChainResource : swapChainResources) {
		swapChainResource.Reset();
	}
	renderTextureResource_.Reset();
	postEffectTextureResource_.Reset();
	gaussianBlurTextureResource_.Reset();
	depthStencilResource.Reset();
	resource.Reset();

	width = newWidth;
	height = newHeight;
	const HRESULT resizeResult = swapChain->ResizeBuffers(
		static_cast<UINT>(GetSwapChainResourcesNum()), width, height,
		swapChainDesc_.Format, swapChainDesc_.Flags);
	assert(SUCCEEDED(resizeResult));

	for (UINT i = 0; i < GetSwapChainResourcesNum(); ++i) {
		const HRESULT getBufferResult = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainResources[i]));
		assert(SUCCEEDED(getBufferResult));
	}

	CreateRenderTargetViews();
	CreateDepthStencilResource();
	CreateDepthStencilView();
	ConfigureViewport();
	ConfigureScissorRect();
	CreateRenderTextureSRV(SrvManager::GetInstance());
	isRenderTextureShaderResource_ = false;
	isPostEffectTextureShaderResource_ = false;
	isGaussianBlurTextureShaderResource_ = false;
	isDepthStencilShaderResource_ = false;
}

void DirectXCommon::CreateDevice()
{
	HRESULT hr;

#if defined(_DEBUG) && defined(ENABLE_D3D12_DEBUG_LAYER)

	ComPtr < ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		//デバックレイヤーを有効化する
		debugController->EnableDebugLayer();
	}
#endif
	//HRESULTはWindows系のエラーコードであり、
	//関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	//初期化の根本的な部分でエラーが出た場合はプログラムが間違ってるか、
	// どうにもできない場合が多いのでassertにしておく
	assert(SUCCEEDED(hr));

	ComPtr < IDXGIAdapter4> useAdapter = nullptr;
	//良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(
		i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i)
	{
		//アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));//取得できないのは一大事
		//ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
		{
			//採用したアダプタの情報をログに出力。wstringの方なので注意
			Logger::Log(StringUtility::ConvertString(std::format(L"Use Adapater:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr;//ソフトウェアアダプタの場合は見なかったことにする
	}
	assert(useAdapter != nullptr);

	//機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] =
	{
	D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
	};

	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };

	//高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i)
	{
		//採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
		//指定した機能レベルでデバイスが生成できたのでログ出力を行ってループを抜ける
		if (SUCCEEDED(hr))
		{
			Logger::Log(std::format("Feature:{}\n", featureLevelStrings[i]));
			break;
		}
	}
	//デバイスの生成がうまくいかなかったので起動できない
	assert(device_ != nullptr);

#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		//ヤバイエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		//緊急時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] =
		{
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE };
		//抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		//指定したメッセージの表示の描画を抑制する
		infoQueue->PushStorageFilter(&filter);

	}
#endif
}

void DirectXCommon::CreateCommandObjects()
{
	//コマンドアロケータ
	for (auto& commandAllocator : commandAllocators_) {
		hr = device_->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(commandAllocator.GetAddressOf())
		);
		assert(SUCCEEDED(hr));
	}

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device_->CreateCommandQueue(&commandQueueDesc,
		IID_PPV_ARGS(&commandQueue));
	//コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	// Command ListはAllocatorとQueueを作った後に一度だけ生成する。
	hr = device_->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocators_[frameIndex_].Get(),
		nullptr,
		IID_PPV_ARGS(&commandList_));
	assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateSwapChain()
{
	// スワップチェーンと二枚のバックバッファを生成する。
	swapChainDesc_.Width = WinApp::kClientWidth;//画面の幅。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc_.Height = WinApp::kClientHeight;//画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//色の形式
	swapChainDesc_.SampleDesc.Count = 1;//マルチサンプルしない
	swapChainDesc_.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;//描画のターゲットとして利用する
	swapChainDesc_.BufferCount = 2;//ダブルバッファ
	swapChainDesc_.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//モニタにうつしたら、中身を破棄
	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	swapChainDesc_.Width = width;
	swapChainDesc_.Height = height;
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), winApp->GetHwnd(), &swapChainDesc_, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));

	//SwapChainからResourceを引っ張ってくる
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	//うまく取得できなければ起動できない
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateDepthStencilResource()
{

	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;//Textureの幅
	resourceDesc.Height = height;//Textureの高さ
	resourceDesc.MipLevels = 1;//mipmapの数
	resourceDesc.DepthOrArraySize = 1;//奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;//DepthStencilとSRVの両方で利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;//サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//DepthStencilとして使う通知

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//VRAM上に作る

	//深度数のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;//1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//フォーマット。Resourceと合わせる

	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	depthStencilResource = resource;


}

void DirectXCommon::CreateDescriptorHeaps()
{
	//RTV,SRV,DSVデスクリプタヒープの生成
	//DescriptorSizeを取得しておく
	descriptorSizeSRV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	//ディスクリプタヒープの生成
	//RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
	// SwapChainの2枚にRenderTextureとPostEffect用の2枚を加え、RTVを5個確保する
	rtvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 5, false);
	//SRV用のヒープでディスクリプタのの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
	srvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	//DSV用のヒープでディスクリプタの数は1。DSVはShader内で触れるものではないので、ShaderVisibleはfalse
	dsvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
}

void DirectXCommon::CreateRenderTargetViews()
{//レンダーターゲットビューの初期化
	 // RTV 設定
	 
	// インクリメントサイズ
	UINT increment = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	 
	// ディスクリプタの先頭
	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	rtvDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc_.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	// 2つのバックバッファに対して RTV を作成
	for (int i = 0; i < 2; ++i)
	{
		assert(swapChainResources[i]);
		device_->CreateRenderTargetView(
			swapChainResources[i].Get(),
			&rtvDesc_,
			handle
		);

		// 次のディスクリプタ位置へ
		rtvHandle_[i] = handle;

		handle.ptr += increment;
	}

	// Sceneを描画するRenderTextureをSwapChainと同じ大きさ・Formatで生成する
	renderTextureResource_ = CreateRenderTextureResource(
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		renderTextureClearColor_);

	// RTVヒープの3個目にRenderTexture用RTVを生成する
	renderTextureRtvHandle_ = handle;
	device_->CreateRenderTargetView(
		renderTextureResource_.Get(),
		&rtvDesc_,
		renderTextureRtvHandle_);

	handle.ptr += increment;

	postEffectTextureResource_ = CreateRenderTextureResource(
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		renderTextureClearColor_);

	postEffectTextureRtvHandle_ = handle;
	device_->CreateRenderTargetView(
		postEffectTextureResource_.Get(),
		&rtvDesc_,
		postEffectTextureRtvHandle_);

	handle.ptr += increment;

	gaussianBlurTextureResource_ = CreateRenderTextureResource(
		width,
		height,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		renderTextureClearColor_);

	gaussianBlurTextureRtvHandle_ = handle;
	device_->CreateRenderTargetView(
		gaussianBlurTextureResource_.Get(),
		&rtvDesc_,
		gaussianBlurTextureRtvHandle_);
}

void DirectXCommon::CreateDepthStencilView()
{
	assert(device_);
	assert(dsvDescriptorHeap);
	assert(depthStencilResource);
	//深度ステンシルビューの初期化
	//描画先のRTVとDSVを設定する
	dsvHandle_ = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	//DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2dTexture

	//DSVHeapの先頭にDSVを作る
	device_->CreateDepthStencilView(depthStencilResource.Get(), &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

}

void DirectXCommon::CreateFence()
{
	hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	//FenceのSignalを待つためのイベントを作成する
	assert(fenceEvent_ != nullptr);
}

void DirectXCommon::ConfigureViewport()
{
	//ビューポート矩形の設定

	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
}

void DirectXCommon::ConfigureScissorRect()
{
	//基本的にはビューポートと同じ短形が構成されるようにする
	scissorRect_.left = 0;
	scissorRect_.right = static_cast<LONG>(width);
	scissorRect_.top = 0;
	scissorRect_.bottom = static_cast<LONG>(height);
}


void DirectXCommon::CreateDxcCompiler()
{
	//DXCコンパイラの生成
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeFixFPS()
{
	//現在時間を記録する
	reference_ = std::chrono::steady_clock::now();

	//1/60秒ぴったりの時間
	kMinTime_ = std::chrono::microseconds(uint64_t(1000000.0f / 60.0f));
	//1/60秒よりわずかに短い時間
	kMinCheckTime_ = std::chrono::microseconds(uint64_t(1000000.0f / 65.0f));
}

void DirectXCommon::UpdateFixFPS()
{
	const std::chrono::steady_clock::time_point target = reference_ + kMinTime_;
	std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
	if (currentTime < target) {
		// Sleepの粒度で大きく遅れないよう、最後の1msだけ短く待つ。
		const std::chrono::steady_clock::time_point sleepUntil = target - std::chrono::milliseconds(1);
		if (currentTime < sleepUntil) {
			std::this_thread::sleep_until(sleepUntil);
		}
		while (std::chrono::steady_clock::now() < target) {
			std::this_thread::yield();
		}
	}
	const std::chrono::steady_clock::time_point frameEndTime = std::chrono::steady_clock::now();
	const std::chrono::duration<float> elapsedTime = frameEndTime - reference_;
	deltaTime_ = std::clamp(elapsedTime.count(), 0.0f, 0.1f);
	reference_ = frameEndTime;
	return;

	//現在時間を取得する
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	//前回記録からの経過時間を取得する
	std::chrono::microseconds elapsed =
		std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

	//1/60秒(よりわずかに短い時間)経ってない場合
	if (elapsed < kMinCheckTime_)
	{//1/60秒経過するまで微小なスリープを繰り返す
		while (std::chrono::steady_clock::now() - reference_ < kMinTime_)
		{
			//1マイクロ秒スリープ
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
	//現在の時間を記録する
	reference_ = std::chrono::steady_clock::now();
}


void DirectXCommon::WaitForGPU() {
	// 1. フェンス値を加算してシグナルを送る
	fenceVal++;
	commandQueue->Signal(fence.Get(), fenceVal);

	// 2. GPUがその値に到達するまで待機する
	if (fence->GetCompletedValue() < fenceVal) {
		fence->SetEventOnCompletion(fenceVal, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}
	for (UINT64& frameFenceValue : frameFenceValues_) {
		frameFenceValue = fenceVal;
	}
}


DirectXCommon::~DirectXCommon() {
	if (fenceEvent_) {
		CloseHandle(fenceEvent_);
	}
}
