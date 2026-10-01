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

	static GPUParticle* GetInstance() {
		static GPUParticle instance;
		return &instance;
	}

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Update(float deltaTime);
	void Draw(const Camera* camera);
	void SetEmitterTranslate(uint32_t emitterIndex, const Vector3& translate);
	void SetEmitterEnabled(uint32_t emitterIndex, bool isEnabled);
	void SetEmitterType(uint32_t emitterIndex, EmitterType type);
	void SetEmitterParticleType(uint32_t emitterIndex, ParticleType type);
	void SetFieldAcceleration(const Vector3& acceleration) { fieldData_->acceleration = acceleration; }
	void SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity);
	void Finalize();

private:
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

	void CreateComputePipeline();
	void CreateGraphicsPipeline();
	void InitializeParticles();

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
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
