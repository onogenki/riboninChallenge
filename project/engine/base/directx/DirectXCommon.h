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

namespace Engine { class SrvManager; }

namespace Engine
{
// DirectX 12の初期化、描画先の切替、GPUへの転送とフレーム終了を管理します。
class DirectXCommon
{
public:

	// @brief Windowの現在サイズを使い、Device・描画先・同期資源を依存順に準備します。
	// @param winApp 初期化済みのWindowです。所有権は移しません。
	// @throws std::invalid_argument Windowが渡されなかった時です。
	// @throws std::runtime_error 必須のDirectX資源を生成できなかった時です。
	void Initialize(WinApp* winApp);
	// @brief Windowサイズが変わった時だけGPU完了を待ち、描画先を作り直します。
	// @throws std::runtime_error SwapChainや必須描画先の再生成に失敗した時です。
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


	// @brief 指定した用途のDescriptor Heapを作り、所有権を呼出し元へ渡します。
	// @param heapType RTV・DSV・SRVなど、登録するDescriptorの種類です。
	// @param numDescriptors 必要な枠数です。0なら作成しません。
	// @param shaderVisible Shaderから参照するHeapだけtrueにします。
	// @return 生成したHeapです。Device未準備や生成失敗時は空です。
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	//SRVの指定番号のCPUデスクリプタハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);

	//SRVの指定番号のGPUデスクリプタハンドルを取得する
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);


	// Shaderの読込・コンパイル・結果取得が失敗した時は、空のBlobを返さず例外で呼び出し元へ伝えます。
	Microsoft::WRL::ComPtr < IDxcBlob> CompileShader(
		//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//Compilerに使用するProfile
		const wchar_t* profile);

	// @brief 読込済み画像の情報からGPU上のTextureを転送待ち状態で作ります。
	// @param metadata 画像の幅・高さ・Format・Mip数です。
	// @return 生成したTextureです。Device未準備や生成失敗時は空です。
	Microsoft::WRL::ComPtr<ID3D12Resource>CreateTextureResource(const DirectX::TexMetadata& metadata);

	// @brief オフスクリーン描画先を作り、RTV登録は呼出し元に任せます。
	// @param width 描画先の横画素数です。0なら作成しません。
	// @param height 描画先の縦画素数です。0なら作成しません。
	// @param format RTVとSRVで使うTexture Formatです。
	// @param clearColor 描画先のClear最適化に使うRGBAです。
	// @return 生成したTextureです。Device未準備や生成失敗時は空です。
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(
		uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

	// @brief SrvManagerの初期化後に、三つの描画先と深度Textureの計四つのSRVを登録します。
	// @param srvManager 番号とHeapを管理する初期化済みのManagerです。所有しません。
	// @throws std::runtime_error 必須TextureやSRV枠が不足した時です。
	void CreateRenderTextureSRV(Engine::SrvManager* srvManager);

	// @brief 画像の各行をGPU転送用BufferへCPU側で詰めます。
	// @param texture 転送先のTextureです。所有しません。
	// @param mipImages 全Mip・配列面の元画素です。
	// @param device 転送先と同じDeviceです。所有しません。
	// @return GPUコピー完了まで保持する中間Bufferです。入力・確保・Map失敗時は空です。
	[[nodiscard]]
	Microsoft::WRL::ComPtr<ID3D12Resource> WriteToIntermediateResource(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device);
	// @brief 全サブリソースのコピー命令と、Shaderから読む状態への遷移を記録します。
	// @param texture コピー先のTextureです。初期状態はCOPY_DESTを想定します。
	// @param intermediateResource CPUが画素を詰めた中間Bufferです。GPU完了まで生存させます。
	// @param numSubresources コピーするMip・配列面の総数です。
	// @param device Textureと同じDeviceです。所有しません。
	// @param commandList コピー命令を記録する開いたListです。所有しません。
	void RecordTextureCopyCommand(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const Microsoft::WRL::ComPtr<ID3D12Resource>& intermediateResource, size_t numSubresources, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	// @brief CPUが書き込めるUpload Bufferを作ります。
	// @param sizeInBytes 必要なバイト数です。0なら作成しません。
	// @return 生成したBufferです。Device未準備や生成失敗時は空です。
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	//getter
	ID3D12Device* GetDevice() const { return device_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList()const { return commandList_.Get(); }

	// GPU命令を実行するキューを返します。所有権はDirectXCommonに残します。
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocators_[frameIndex_].Get(); }
	// 描画中のSwapChain枠とフレーム番号です。枠のFence待ち後だけ同じUpload領域を再利用できます。
	UINT GetFrameIndex() const { return frameIndex_; }
	uint64_t GetFrameSerial() const { return frameSerial_; }
	uint32_t GetRenderTextureSrvIndex() const { return renderTextureSrvIndex_; }
	uint32_t GetPostEffectTextureSrvIndex() const { return postEffectTextureSrvIndex_; }
	uint32_t GetGaussianBlurTextureSrvIndex() const { return gaussianBlurTextureSrvIndex_; }
	uint32_t GetDepthStencilSrvIndex() const { return depthStencilSrvIndex_; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilViewHandle() const { return dsvHandle_; }
	// 描画先の再生成にも使う、現在のクライアント領域の幅・高さです。
	uint32_t GetClientWidth() const { return width_; }
	uint32_t GetClientHeight() const { return height_; }
	float GetDeltaTime() const { return deltaTime_; }

	//最大SRV数(最大テクスチャ枚数)
	static const uint32_t kMaxSRVCount;

	// SwapChainの設定と配列が同じ枚数を使うよう、共通の枚数を返します。
	size_t GetSwapChainResourcesNum() const { return kSwapChainBufferCount; }

	//GPU待ち関数
	void WaitForGPU();
	// @brief 描画先を同期的に読み戻します。GPU待ちが入るため連続撮影には使いません。
	// @param pixels 成功時にBGRA順の画素を受け取る出力先です。
	// @param outWidth 成功時に画像の幅を受け取る出力先です。
	// @param outHeight 成功時に画像の高さを受け取る出力先です。
	// @param usePostEffectTexture trueならポストエフェクト後、falseならScene描画先を読みます。
	// @return GPUから画素を取得できた時だけtrueです。
	bool CaptureGameTexturePixels(std::vector<unsigned char>& pixels, int& outWidth, int& outHeight, bool usePostEffectTexture);
	// @brief 描画先のコピーを空きスロットへ予約し、後のフレームで取得できるようにします。
	// @param usePostEffectTexture trueならポストエフェクト後、falseならScene描画先を予約します。
	// @return 空きスロットとGPU資源を確保して予約できた時だけtrueです。
	bool QueueGameTextureCapture(bool usePostEffectTexture);
	// @brief GPUが完了した最古の撮影予約から画素を取り出します。
	// @param pixels 成功時にBGRA順の画素を受け取る出力先です。
	// @param outWidth 成功時に画像の幅を受け取る出力先です。
	// @param outHeight 成功時に画像の高さを受け取る出力先です。
	// @return 未完了ならfalseです。予約を消費した時はtrueで、取得失敗なら画素は空です。
	bool TryGetGameTextureCapturePixels(std::vector<unsigned char>& pixels, int& outWidth, int& outHeight);

	// 転送用資源とCommand Listを準備できた時だけコピー・GPU待ちを行い、SRV登録へ進める。
	bool ExecuteTextureTransfer(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);

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

	// 撮影一件のGPUコピーと完了順を所有し、未完了の資源を再利用させません。
	struct CaptureReadbackSlot {
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
		Microsoft::WRL::ComPtr<ID3D12Resource> readbackResource_;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
		UINT64 totalBytes_ = 0;
		UINT64 fenceValue_ = 0;
		UINT64 sequence_ = 0;
		int width_ = 0;
		int height_ = 0;
		bool pending_ = false;
	};

	static constexpr size_t kCaptureReadbackSlotCount_ = 4;
	// 表示用Buffer・Command Allocator・Fence・RTVは同じ二枚分を所有します。
	static constexpr size_t kSwapChainBufferCount = 2;

	DirectXCommon() = default;
	~DirectXCommon();

	Microsoft::WRL::ComPtr<ID3D12Device>device_;
	Microsoft::WRL::ComPtr<IDXGIFactory7>dxgiFactory_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators_[kSwapChainBufferCount];
	Microsoft::WRL::ComPtr < ID3D12CommandQueue> commandQueue_;
	Microsoft::WRL::ComPtr < ID3D12Fence> fence_;
	Microsoft::WRL::ComPtr < IDxcUtils> dxcUtils_;
	Microsoft::WRL::ComPtr < IDxcCompiler3> dxcCompiler_;
	//現時点でincludeはしないが、includeに対応するための設定を行っておく
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

	Microsoft::WRL::ComPtr < ID3D12GraphicsCommandList> commandList_;

	// 描画・撮影・転送のSignalで共有し、投入順に単調増加するFence値です。
	UINT64 signaledFenceValue_ = 0;
	UINT64 frameFenceValues_[kSwapChainBufferCount] = {};
	UINT frameIndex_ = 0;
	uint64_t frameSerial_ = 0;
	std::array<CaptureReadbackSlot, kCaptureReadbackSlotCount_> captureReadbackSlots_{};
	UINT64 captureSequence_ = 0;

	//スワップチェーンを生成する
	Microsoft::WRL::ComPtr < IDXGISwapChain4> swapChain_;
	Microsoft::WRL::ComPtr < ID3D12Resource> swapChainResources_[kSwapChainBufferCount];

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc_{};
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_{};

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;

	// Fence作成時だけ渡す初期値です。投入済みの値はsignaledFenceValue_が管理します。
	uint64_t fenceValue_ = 0;

	HANDLE fenceEvent_ = nullptr;

	// 深度とステンシルのTextureを一つだけ所有し、DSVとSRVの両方から参照します。
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
	uint32_t depthStencilSrvIndex_ = UINT32_MAX;
	bool isDepthStencilShaderResource_ = false;
	UINT descriptorSizeSRV_ = 0;
	UINT descriptorSizeRTV_ = 0;
	UINT descriptorSizeDSV_ = 0;

	//描画先のRTVとDSVを設定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_[kSwapChainBufferCount]{};

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

	//シザー短形
	D3D12_RECT scissorRect_{};

	//WindowsAPI
	WinApp* winApp_ = nullptr;

	//FPS固定初期化
	void InitializeFixFPS();
	//FPS固定更新
	void UpdateFixFPS();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index);

	//記録時間(FPS固定用)
	std::chrono::steady_clock::time_point reference_{};
	std::chrono::microseconds minFrameTime_{};
	float deltaTime_ = 1.0f / 60.0f;

	uint32_t width_ = WinApp::kClientWidth; // 幅・高さをメンバに
	uint32_t height_ = WinApp::kClientHeight;


};
}
