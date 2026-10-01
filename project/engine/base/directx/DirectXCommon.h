#pragma once
#include <d3d12.h>
#include<dxgi1_6.h>
#include<wrl.h>
#include <DirectXMath.h>
#include "WinApp.h"
#include <array>
#include "externals/DirectXTex/DirectXTex.h"
#include<string>
#include <vector>
#include <dxcapi.h>
#include<chrono>
#include "Vector4.h"

class DirectXCommon
{
public:

	void Initialize(WinApp* winApp);
	void ResizeIfNeeded();

	//描画前処理
	void PreDraw(const Vector4* clearColorOverride = nullptr);
	// Scene描画後、ImGuiを描画するSwapChainへ描画先を切り替える
	void PreDrawForPostEffectTexture();
	void PreDrawForDepthBasedOutlineTexture();
	void PreDrawForGaussianHorizontalTexture();
	void PreDrawForGaussianVerticalTexture();
	void PreDrawForSwapChain(bool usePostEffectTexture = false);
	//描画後処理
	void PostDraw();

	static DirectXCommon* GetInstance() {
		static DirectXCommon instance;
		return &instance;
	}

	DirectXCommon(const DirectXCommon&) = delete;
	DirectXCommon& operator=(const DirectXCommon&) = delete;


	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	//SRVの指定番号のCPUデスクリプタハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);

	//SRVの指定番号のGPUデスクリプタハンドルを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);


	Microsoft::WRL::ComPtr < IDxcBlob> CompileShader(
		//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//Compilerに使用するProfile
		const wchar_t* profile);

	//テクスチャリソースの生成
	Microsoft::WRL::ComPtr<ID3D12Resource>CreateTextureResource(const DirectX::TexMetadata& metadata);

	// オフスクリーン描画の描画先として使うRenderTextureを生成する
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(
		uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

	// SrvManagerの初期化後にRenderTextureを読み取るためのSRVを生成する
	void CreateRenderTextureSRV(class SrvManager* srvManager);

	//CPUのMap/memcpyだけを行う
	[[nodiscard]]
	Microsoft::WRL::ComPtr<ID3D12Resource> WriteToIntermediateResource(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device);
	//GPUのCopyTextureRegionだけを行う
	void RecordTextureCopyCommand(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const Microsoft::WRL::ComPtr<ID3D12Resource>& intermediateResource, size_t numSubresources, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	//どこからでも使えるバッファ生成関数として公開する
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	//getter
	ID3D12Device* GetDevice() const { return device_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList()const { return commandList_.Get(); }

	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocators_[frameIndex_].Get(); }
	// 描画中のSwapChain枠とフレーム番号です。枠のFence待ち後だけ同じUpload領域を再利用できます。
	UINT GetFrameIndex() const { return frameIndex_; }
	uint64_t GetFrameSerial() const { return frameSerial_; }
	uint32_t GetRenderTextureSrvIndex() const { return renderTextureSrvIndex_; }
	uint32_t GetPostEffectTextureSrvIndex() const { return postEffectTextureSrvIndex_; }
	uint32_t GetGaussianBlurTextureSrvIndex() const { return gaussianBlurTextureSrvIndex_; }
	uint32_t GetDepthStencilSrvIndex() const { return depthStencilSrvIndex_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilViewHandle() const { return dsvHandle_; }
	uint32_t GetClientWidth() const { return width; }
	uint32_t GetClientHeight() const { return height; }
	float GetDeltaTime() const { return deltaTime_; }

	//最大SRV数(最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;

	//スワップチェーンリソースの数を取得
	size_t GetSwapChainResourcesNum() const { return 2;}

	//GPU待ち関数
	void WaitForGPU();
	bool CaptureGameTexturePixels(std::vector<unsigned char>& pixels, int& outWidth, int& outHeight, bool usePostEffectTexture);
	bool QueueGameTextureCapture(bool usePostEffectTexture);
	bool TryGetGameTextureCapturePixels(std::vector<unsigned char>& pixels, int& outWidth, int& outHeight);

	// テクスチャの転送コマンドを実行して完了を待つ関数
	void ExecuteTextureTransfer(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);

private:
	// DirectXの起動時と画面サイズ変更時にだけ呼ぶ内部初期化処理。
	void CreateDevice();
	void CreateCommandObjects();
	void CreateSwapChain();
	void CreateDepthStencilResource();
	void CreateDescriptorHeaps();
	void CreateRenderTargetViews();
	void CreateDepthStencilView();
	void CreateFence();
	void ConfigureViewport();
	void ConfigureScissorRect();
	void CreateDxcCompiler();

	struct CaptureReadbackSlot {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
		Microsoft::WRL::ComPtr<ID3D12Resource> readbackResource;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
		UINT64 totalBytes = 0;
		UINT64 fenceValue = 0;
		UINT64 sequence = 0;
		int width = 0;
		int height = 0;
		bool pending = false;
	};

	static constexpr size_t kCaptureReadbackSlotCount_ = 4;

	DirectXCommon() = default;
	~DirectXCommon();

	Microsoft::WRL::ComPtr<ID3D12Device>device_;
	Microsoft::WRL::ComPtr<IDXGIFactory7>dxgiFactory;
	Microsoft::WRL::ComPtr < ID3D12Debug1> debugController;
	Microsoft::WRL::ComPtr < IDXGIAdapter4> useAdapter;
	Microsoft::WRL::ComPtr < ID3D12InfoQueue> infoQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators_[2];
	Microsoft::WRL::ComPtr < ID3D12CommandQueue> commandQueue;
	Microsoft::WRL::ComPtr < ID3D12Resource> resource;
	Microsoft::WRL::ComPtr < ID3D12Fence> fence;
	Microsoft::WRL::ComPtr < IDxcUtils> dxcUtils;
	Microsoft::WRL::ComPtr < IDxcCompiler3> dxcCompiler;
	//現時点でincludeはしないが、includeに対応するための設定を行っておく
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;

	Microsoft::WRL::ComPtr < ID3D12GraphicsCommandList> commandList_;

	//フェンス値
	UINT64 fenceVal = 0;
	UINT64 frameFenceValues_[2] = {};
	UINT frameIndex_ = 0;
	uint64_t frameSerial_ = 0;
	std::array<CaptureReadbackSlot, kCaptureReadbackSlotCount_> captureReadbackSlots_{};
	UINT64 captureSequence_ = 0;

	//スワップチェーンを生成する
	Microsoft::WRL::ComPtr < IDXGISwapChain4> swapChain;
	Microsoft::WRL::ComPtr < ID3D12Resource> swapChainResources[2];

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc_{};
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_{};

	//Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;

	//フェンス生成
	uint64_t fenceValue_ = 0;

	HANDLE fenceEvent_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;
	uint32_t depthStencilSrvIndex_ = UINT32_MAX;
	bool isDepthStencilShaderResource_ = false;
	UINT descriptorSizeSRV = 0;
	UINT descriptorSizeRTV = 0;
	UINT descriptorSizeDSV = 0;

	//描画先のRTVとDSVを設定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_[2]{};

	// Sceneの描画先として使うオフスクリーンRenderTexture
	Microsoft::WRL::ComPtr<ID3D12Resource> renderTextureResource_;
	D3D12_CPU_DESCRIPTOR_HANDLE renderTextureRtvHandle_{};
	uint32_t renderTextureSrvIndex_ = UINT32_MAX;
	bool isRenderTextureShaderResource_ = false;
	Microsoft::WRL::ComPtr<ID3D12Resource> postEffectTextureResource_;
	D3D12_CPU_DESCRIPTOR_HANDLE postEffectTextureRtvHandle_{};
	uint32_t postEffectTextureSrvIndex_ = UINT32_MAX;
	bool isPostEffectTextureShaderResource_ = false;
	Microsoft::WRL::ComPtr<ID3D12Resource> gaussianBlurTextureResource_;
	D3D12_CPU_DESCRIPTOR_HANDLE gaussianBlurTextureRtvHandle_{};
	uint32_t gaussianBlurTextureSrvIndex_ = UINT32_MAX;
	bool isGaussianBlurTextureShaderResource_ = false;
	const Vector4 renderTextureClearColor_{ 1.0f, 0.0f, 0.0f, 1.0f };

	//ビューポート
	D3D12_VIEWPORT viewport_{};

	HRESULT hr = S_OK;

	//シザー短形
	D3D12_RECT scissorRect_{};

	//WindowsAPI
	WinApp* winApp = nullptr;

	//FPS固定初期化
	void InitializeFixFPS();
	//FPS固定更新
	void UpdateFixFPS();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	//記録時間(FPS固定用)
	std::chrono::steady_clock::time_point reference_{};
	std::chrono::microseconds kMinTime_{};
	std::chrono::microseconds kMinCheckTime_{};
	float deltaTime_ = 1.0f / 60.0f;

	uint32_t width = WinApp::kClientWidth; // 幅・高さをメンバに
	uint32_t height = WinApp::kClientHeight;


};
