#pragma once
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"

class Camera;

// Compute Shaderで粒子を更新し、描画用バッファとEmitter設定を管理します。
class GPUParticle
{
public:
	static constexpr uint32_t kMaxParticles = 1024;
	static constexpr uint32_t kMaxEmitters = 2;

	enum class EmitterType : uint32_t
	{
		Sphere,
		Box,
		Cone,
		Mesh,
		Mix,
	};

	enum class ParticleType : uint32_t
	{
		Billboard,
		Trail,
	};

	// FrameworkとDebug Sceneで共有する本体への、所有権のないポインタを返します。
	static GPUParticle* GetInstance() {
		static GPUParticle instance;
		return &instance;
	}

	// Frameworkが初期化した二つの窓口を借り、粒子用資源を作ります。SRVの空きが7枠必要です。
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	// Debug Sceneが使う固定画像を読み、失敗時は描画できない状態を保ちます。
	bool PrepareTexture();
	// deltaTime秒だけEmitterを進め、発生とGPU上の粒子更新をこの順に行います。
	void Update(float deltaTime);
	// 指定Cameraと固定画像の準備がそろった時だけ粒子を描きます。
	void Draw(const Camera* camera);
	// emitterIndex番の発生位置をワールド座標で指定します。範囲外の番号は無視します。
	void SetEmitterTranslate(uint32_t emitterIndex, const Vector3& translate);
	// emitterIndex番の発生を有効・無効にします。範囲外の番号は無視します。
	void SetEmitterEnabled(uint32_t emitterIndex, bool isEnabled);
	// emitterIndex番の発生形状を指定します。範囲外の番号は無視します。
	void SetEmitterType(uint32_t emitterIndex, EmitterType type);
	// emitterIndex番が作る粒子の描画種類を指定します。範囲外の番号は無視します。
	void SetEmitterParticleType(uint32_t emitterIndex, ParticleType type);
	// 初期化後、全粒子に適用する加速度をGPU用データへ書き込みます。
	void SetFieldAcceleration(const Vector3& acceleration) { fieldData_->acceleration = acceleration; }
	// 初期化後、粒子Shaderへ渡す光の色・方向・強さを設定します。方向は呼び出し元の値をそのまま使います。
	void SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity);
	// Framework終了時に、粒子が使ったSRV番号とGPU資源を返却します。
	void Finalize();

private:
	// 更新Shaderが同時に使う五つのUAV数を、Root Signatureと更新後バリアで共有します。
	static constexpr uint32_t kSimulationUavCount = 5;

	enum ComputeRootParameter : uint32_t
	{
		kComputeRootParameterUav,
		kComputeRootParameterEmitter,
		kComputeRootParameterPerFrame,
		kComputeRootParameterField,
		kComputeRootParameterCount,
	};

	enum GraphicsRootParameter : uint32_t
	{
		kGraphicsRootParameterPerView,
		kGraphicsRootParameterParticle,
		kGraphicsRootParameterTexture,
		kGraphicsRootParameterDirectionalLight,
		kGraphicsRootParameterCount,
	};

	struct Particle
	{
		Vector3 translate;
		Vector3 scale;
		float lifeTime;
		Vector3 velocity;
		float currentTime;
		Vector4 color;
		uint32_t particleType;
		uint32_t emitterType;
	};

	struct PerView
	{
		Matrix4x4 viewProjectionMatrix;
		Matrix4x4 billboardMatrix;
	};

	struct Emitter
	{
		Vector3 translate;
		float radius;
		Vector3 boxSize;
		uint32_t count;
		float frequency;
		float frequencyTime;
		uint32_t emit;
		uint32_t type;
		uint32_t particleType;
		uint32_t padding[3];
	};

	struct PerFrame
	{
		float time;
		float deltaTime;
	};

	struct Field
	{
		Vector3 acceleration;
		float drag;
	};

	struct DirectionalLight
	{
		Vector4 color;
		Vector3 direction;
		float intensity;
	};

	struct VertexData
	{
		Vector4 position;
		float texcoord[2];
	};

	GPUParticle() = default;
	~GPUParticle() = default;
	GPUParticle(const GPUParticle&) = delete;
	GPUParticle& operator=(const GPUParticle&) = delete;

	// 粒子計算用バッファ、Shaderから見るSRV/UAV、描画・制御用バッファを依存順に準備します。
	void CreateParticleBuffers();
	// 五種類の粒子バッファを、同じDefault Heap・UAV状態で作り、失敗した名前を伝えます。
	void CreateParticleBufferResource(uint64_t sizeInBytes, Microsoft::WRL::ComPtr<ID3D12Resource>& resource, const char* resourceName) const;
	void CreateParticleDescriptors();
	void CreateDrawAndControlBuffers();
	void CreateComputePipeline();
	void CreateGraphicsPipeline();
	void InitializeParticles();
	// CPUでEmitterの発生時刻を決め、確定した値を使ってGPU計算を進めます。
	void UpdateEmitterTiming(float deltaTime);
	void DispatchParticleSimulation();

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	// 固定画像の読込に成功した時だけDrawでSRVを取得します。
	bool isTextureReady_ = false;
	uint32_t particleSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t activeParticleIndicesSrvIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t particleUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t freeListIndexUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t freeListUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t activeParticleIndicesUavIndex_ = SrvManager::kInvalidSrvIndex;
	uint32_t drawArgumentsUavIndex_ = SrvManager::kInvalidSrvIndex;
	Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> activeParticleIndicesResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> drawArgumentsResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kMaxEmitters> emitterResources_;
	Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> fieldResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	PerView* perViewData_ = nullptr;
	std::array<Emitter*, kMaxEmitters> emitterData_{};
	PerFrame* perFrameData_ = nullptr;
	Field* fieldData_ = nullptr;
	DirectionalLight* directionalLightData_ = nullptr;
	std::array<bool, kMaxEmitters> isEmitterEnabled_{};
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> initializePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> graphicsRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12CommandSignature> drawCommandSignature_;
};
