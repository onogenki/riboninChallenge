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

// WinAppの画面サイズを保持し、依存順にGPU資源を作って描画開始に備えます。
void DirectXCommon::Initialize(WinApp* winApp)
{
	//NULL検出
	assert(winApp);

	//メンバ変数に記録
	winApp_ = winApp;
	width_ = winApp->GetClientWidth();
	height_ = winApp->GetClientHeight();

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

// WinAppの現在サイズを確認し、変更がある時だけGPU待ち後に描画先を作り直します。
void DirectXCommon::ResizeIfNeeded()
{
	const uint32_t newWidth = winApp_->GetClientWidth();
	const uint32_t newHeight = winApp_->GetClientHeight();
	if (newWidth == 0 || newHeight == 0 || (newWidth == width_ && newHeight == height_)) {
		return;
	}

	WaitForGPU();
	for (auto& swapChainResource : swapChainResources_) {
		swapChainResource.Reset();
	}
	renderTextureResource_.Reset();
	postEffectTextureResource_.Reset();
	gaussianBlurTextureResource_.Reset();
	depthStencilResource_.Reset();

	width_ = newWidth;
	height_ = newHeight;
	const HRESULT resizeResult = swapChain_->ResizeBuffers(
		static_cast<UINT>(GetSwapChainResourcesNum()), width_, height_,
		swapChainDesc_.Format, swapChainDesc_.Flags);
	assert(SUCCEEDED(resizeResult));

	for (UINT i = 0; i < GetSwapChainResourcesNum(); ++i) {
		const HRESULT getBufferResult = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
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

// DXGI FactoryとDeviceを作り、以後のSwapChain・GPU資源生成の土台を用意します。
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
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
	//初期化の根本的な部分でエラーが出た場合はプログラムが間違ってるか、
	// どうにもできない場合が多いのでassertにしておく
	assert(SUCCEEDED(hr));

	ComPtr < IDXGIAdapter4> useAdapter = nullptr;
	//良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(
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
			Logger::Log(StringUtility::ConvertString(std::format(L"Use Adapter:{}\n", adapterDesc.Description)));
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

// GPU命令の記録先と実行キューを生成し、各フレームの描画を開始できるようにします。
void DirectXCommon::CreateCommandObjects()
{
	// 生成結果はこの初期化処理の中だけで使い、別フレームへ持ち越しません。
	HRESULT hr = S_OK;
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
		IID_PPV_ARGS(&commandQueue_));
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

// 作成済みの実行キューとWinAppのウィンドウを結び、表示用の二枚の描画先を作ります。
void DirectXCommon::CreateSwapChain()
{
	// SwapChainとBack Bufferの生成結果を、この処理内で順に確認します。
	HRESULT hr = S_OK;
	// スワップチェーンと二枚のバックバッファを生成する。
	swapChainDesc_.Width = WinApp::kClientWidth;//画面の幅。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc_.Height = WinApp::kClientHeight;//画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//色の形式
	swapChainDesc_.SampleDesc.Count = 1;//マルチサンプルしない
	swapChainDesc_.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;//描画のターゲットとして利用する
	swapChainDesc_.BufferCount = static_cast<UINT>(kSwapChainBufferCount);//ダブルバッファ
	swapChainDesc_.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;//モニタにうつしたら、中身を破棄
	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	swapChainDesc_.Width = width_;
	swapChainDesc_.Height = height_;
	hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), winApp_->GetHwnd(), &swapChainDesc_, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// 設定した枚数だけBack Bufferを取得し、後続のRTV作成と配列の範囲を一致させます。
	for (UINT index = 0; index < kSwapChainBufferCount; ++index) {
		hr = swapChain_->GetBuffer(index, IID_PPV_ARGS(&swapChainResources_[index]));
		assert(SUCCEEDED(hr));
	}
}

// 現在の画面サイズで深度とステンシルを共有できるTextureを作ります。
void DirectXCommon::CreateDepthStencilResource()
{

	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width_;//Textureの幅
	resourceDesc.Height = height_;//Textureの高さ
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
		IID_PPV_ARGS(&depthStencilResource_));
	assert(SUCCEEDED(hr));
}

// SwapChainとオフスクリーン描画先のRTVを、生成する枚数に合わせて確保します。
void DirectXCommon::CreateDescriptorHeaps()
{
	//RTV,SRV,DSVデスクリプタヒープの生成
	//DescriptorSizeを取得しておく
	descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	//ディスクリプタヒープの生成
	// RenderTexture・PostEffect・GaussianBlurの3枚とSwapChainの枚数を合計します。
	// RTVはShaderから直接参照しないため、ShaderVisibleはfalseです。
	constexpr UINT kOffscreenRenderTargetCount = 3;
	const UINT renderTargetCount = static_cast<UINT>(GetSwapChainResourcesNum()) + kOffscreenRenderTargetCount;
	rtvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, renderTargetCount, false);
	//SRV用のヒープでディスクリプタのの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
	srvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxSRVCount, true);
	//DSV用のヒープでディスクリプタの数は1。DSVはShader内で触れるものではないので、ShaderVisibleはfalse
	dsvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
}

// SwapChainと各オフスクリーンTextureへRTVを割り当て、描画先を使えるようにします。
void DirectXCommon::CreateRenderTargetViews()
{//レンダーターゲットビューの初期化
	 // RTV 設定
	 
	// インクリメントサイズ
	UINT increment = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	 
	// ディスクリプタの先頭
	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	rtvDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc_.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	// SwapChainが所有する二枚のBack BufferそれぞれへRTVを作成します。
	for (UINT i = 0; i < kSwapChainBufferCount; ++i)
	{
		assert(swapChainResources_[i]);
		device_->CreateRenderTargetView(
			swapChainResources_[i].Get(),
			&rtvDesc_,
			handle
		);

		// 次のディスクリプタ位置へ
		rtvHandle_[i] = handle;

		handle.ptr += increment;
	}

	// Sceneを描画するRenderTextureをSwapChainと同じ大きさ・Formatで生成する
	renderTextureResource_ = CreateRenderTextureResource(
		width_,
		height_,
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
		width_,
		height_,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		renderTextureClearColor_);

	postEffectTextureRtvHandle_ = handle;
	device_->CreateRenderTargetView(
		postEffectTextureResource_.Get(),
		&rtvDesc_,
		postEffectTextureRtvHandle_);

	handle.ptr += increment;

	gaussianBlurTextureResource_ = CreateRenderTextureResource(
		width_,
		height_,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		renderTextureClearColor_);

	gaussianBlurTextureRtvHandle_ = handle;
	device_->CreateRenderTargetView(
		gaussianBlurTextureResource_.Get(),
		&rtvDesc_,
		gaussianBlurTextureRtvHandle_);
}

// 深度TextureとDSV Heapを結び、Scene描画時に深度・ステンシルを使えるようにします。
void DirectXCommon::CreateDepthStencilView()
{
	assert(device_);
	assert(dsvDescriptorHeap_);
	assert(depthStencilResource_);
	//深度ステンシルビューの初期化
	//描画先のRTVとDSVを設定する
	dsvHandle_ = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	//DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2dTexture

	//DSVHeapの先頭にDSVを作る
	device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());

}

// 描画・撮影・転送が共有するFenceを生成し、GPU完了待ちのEventを用意します。
void DirectXCommon::CreateFence()
{
	// Fenceの成否を生成時だけ判定し、後続フレームに一時結果を残しません。
	const HRESULT hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	assert(SUCCEEDED(hr));

	fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	//FenceのSignalを待つためのイベントを作成する
	assert(fenceEvent_ != nullptr);
}

// 現在の画面サイズにViewportを合わせ、描画範囲がResize後もずれないようにします。
void DirectXCommon::ConfigureViewport()
{
	//ビューポート矩形の設定

	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport_.Width = static_cast<float>(width_);
	viewport_.Height = static_cast<float>(height_);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
}

// 現在の画面サイズで切り取り範囲を更新し、画面外への描画を防ぎます。
void DirectXCommon::ConfigureScissorRect()
{
	//基本的にはビューポートと同じ短形が構成されるようにする
	scissorRect_.left = 0;
	scissorRect_.right = static_cast<LONG>(width_);
	scissorRect_.top = 0;
	scissorRect_.bottom = static_cast<LONG>(height_);
}


// シェーダー読込とコンパイルに必要なDXC一式を、Shader使用前に準備します。
void DirectXCommon::CreateDxcCompiler()
{
	// DXCの各生成結果をこの処理内で確認し、共有状態にしません。
	HRESULT hr = S_OK;
	//DXCコンパイラの生成
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr));

	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeFixFPS()
{
	//現在時間を記録する
	reference_ = std::chrono::steady_clock::now();

	//1/60秒ぴったりの時間
	kMinTime_ = std::chrono::microseconds(uint64_t(1000000.0f / 60.0f));
}

// 描画後に目標時刻まで待ち、次フレームで使う実経過時間を更新します。
// 先にreturnしていた旧待機処理は実行されないため、この一経路だけを残します。
void DirectXCommon::UpdateFixFPS()
{
	const std::chrono::steady_clock::time_point target = reference_ + kMinTime_;
	const std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
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
}


// キューに送った命令の完了をFenceで待ち、転送資源などを安全に再利用します。
void DirectXCommon::WaitForGPU() {
	// 1. フェンス値を加算してシグナルを送る
	signaledFenceValue_++;
	commandQueue_->Signal(fence_.Get(), signaledFenceValue_);

	// 2. GPUがその値に到達するまで待機する
	if (fence_->GetCompletedValue() < signaledFenceValue_) {
		fence_->SetEventOnCompletion(signaledFenceValue_, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}
	for (UINT64& frameFenceValue : frameFenceValues_) {
		frameFenceValue = signaledFenceValue_;
	}
}


DirectXCommon::~DirectXCommon() {
	if (fenceEvent_) {
		CloseHandle(fenceEvent_);
	}
}
