#include "GPUParticle.h"
#include "Camera.h"
#include "Logger.h"
#include "MyMath.h"
#include "TextureManager.h"
#include <stdexcept>
#include <string>

using namespace MyMath;

namespace
{
	constexpr const char* kParticleTexturePath = "Resources/debug/circle2.png";
}

// 粒子のGPU資源を順に作り、Sceneが後から固定画像を準備できる状態にします。
void GPUParticle::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	// 未完成のDescriptorを使ってGPU命令へ進まないよう、必要な依存と7枠を先に確認します。
	if (!dxCommon || !dxCommon->GetDevice() || !dxCommon->GetCommandList() || !srvManager ||
		!srvManager->CanAllocate(7)) {
		throw std::runtime_error("GPUParticle: DirectX resources or seven SRV slots are unavailable");
	}

	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	isTextureReady_ = false;
	CreateParticleBuffers();
	CreateParticleDescriptors();
	CreateComputePipeline();
	CreateGraphicsPipeline();
	CreateDrawAndControlBuffers();
	InitializeParticles();
}

// Debug Scene開始時に固定画像を読み、失敗時は未登録SRVをDrawで取得しないようにします。
bool GPUParticle::PrepareTexture()
{
	isTextureReady_ = TextureManager::GetInstance()->LoadTexture(kParticleTexturePath);
	if (!isTextureReady_) {
		Logger::Log("GPUParticle: particle texture could not be loaded: " + std::string(kParticleTexturePath));
	}
	return isTextureReady_;
}

// Compute Shaderが更新する粒子と空き枠・描画Indexの保存先を作ります。
void GPUParticle::CreateParticleBuffers()
{
	CreateParticleBufferResource(sizeof(Particle) * kMaxParticles, particleResource_, "particle");
	CreateParticleBufferResource(sizeof(int32_t), freeListIndexResource_, "free list index");
	CreateParticleBufferResource(sizeof(int32_t) * kMaxParticles, freeListResource_, "free list");
	CreateParticleBufferResource(sizeof(uint32_t) * kMaxParticles, activeParticleIndicesResource_, "active indices");
	CreateParticleBufferResource(sizeof(uint32_t) * 4, drawArgumentsResource_, "draw arguments");
}

// UAVとして使う粒子バッファの共通設定を一か所に保ち、種類ごとの設定ずれを防ぎます。
void GPUParticle::CreateParticleBufferResource(
	uint64_t sizeInBytes,
	Microsoft::WRL::ComPtr<ID3D12Resource>& resource,
	const char* resourceName) const
{
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	const HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&resource));
	if (FAILED(hr) || !resource) {
		throw std::runtime_error(std::string("GPUParticle: ") + resourceName + " buffer creation failed");
	}
}

// 粒子バッファをCompute Shaderから読み書きできるよう、SRV/UAVを登録します。
void GPUParticle::CreateParticleDescriptors()
{
	particleSrvIndex_ = srvManager_->Allocate();
	activeParticleIndicesSrvIndex_ = srvManager_->Allocate();
	particleUavIndex_ = srvManager_->Allocate();
	freeListIndexUavIndex_ = srvManager_->Allocate();
	freeListUavIndex_ = srvManager_->Allocate();
	activeParticleIndicesUavIndex_ = srvManager_->Allocate();
	drawArgumentsUavIndex_ = srvManager_->Allocate();
	// 一つでも確保できなければDescriptorを登録せず、取得済み番号はFinalizeで返します。
	if (particleSrvIndex_ == SrvManager::kInvalidSrvIndex ||
		activeParticleIndicesSrvIndex_ == SrvManager::kInvalidSrvIndex ||
		particleUavIndex_ == SrvManager::kInvalidSrvIndex ||
		freeListIndexUavIndex_ == SrvManager::kInvalidSrvIndex ||
		freeListUavIndex_ == SrvManager::kInvalidSrvIndex ||
		activeParticleIndicesUavIndex_ == SrvManager::kInvalidSrvIndex ||
		drawArgumentsUavIndex_ == SrvManager::kInvalidSrvIndex) {
		throw std::runtime_error("GPUParticle: seven SRV/UAV slots could not be allocated");
	}
	srvManager_->CreateSRVforStructuredBuffer(particleSrvIndex_, particleResource_.Get(), kMaxParticles, sizeof(Particle));
	srvManager_->CreateSRVforStructuredBuffer(activeParticleIndicesSrvIndex_, activeParticleIndicesResource_.Get(), kMaxParticles, sizeof(uint32_t));
	srvManager_->CreateUAVforStructuredBuffer(particleUavIndex_, particleResource_.Get(), kMaxParticles, sizeof(Particle));
	srvManager_->CreateUAVforStructuredBuffer(freeListIndexUavIndex_, freeListIndexResource_.Get(), 1, sizeof(int32_t));
	srvManager_->CreateUAVforStructuredBuffer(freeListUavIndex_, freeListResource_.Get(), kMaxParticles, sizeof(int32_t));
	srvManager_->CreateUAVforStructuredBuffer(activeParticleIndicesUavIndex_, activeParticleIndicesResource_.Get(), kMaxParticles, sizeof(uint32_t));
	srvManager_->CreateUAVforStructuredBuffer(drawArgumentsUavIndex_, drawArgumentsResource_.Get(), 4, sizeof(uint32_t));
}

// 板ポリゴンとCamera・Emitter・時間・外力・Lightの初期値をGPUへ渡します。
void GPUParticle::CreateDrawAndControlBuffers()
{
	// 各Upload Bufferの生成とMapを同じ順序で確認し、空ポインタへの書込みを避けます。
	auto createMappedBuffer = [this](size_t sizeInBytes, auto& resource, auto*& mappedData, const char* resourceName)
	{
		resource = dxCommon_->CreateBufferResource(sizeInBytes);
		if (!resource) {
			throw std::runtime_error(std::string("GPUParticle: ") + resourceName + " buffer creation failed");
		}
		const HRESULT mapResult = resource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
		if (FAILED(mapResult) || !mappedData) {
			throw std::runtime_error(std::string("GPUParticle: ") + resourceName + " buffer mapping failed");
		}
	};
	VertexData* vertexData = nullptr;
	createMappedBuffer(sizeof(VertexData) * 6, vertexResource_, vertexData, "vertex");
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	vertexData[0] = { { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } };
	vertexData[1] = { { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } };
	vertexData[2] = { {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } };
	vertexData[3] = { { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } };
	vertexData[4] = { {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } };
	vertexData[5] = { {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } };

	createMappedBuffer(sizeof(PerView), perViewResource_, perViewData_, "per-view");
	for (uint32_t emitterIndex = 0; emitterIndex < kMaxEmitters; ++emitterIndex)
	{
		createMappedBuffer(sizeof(Emitter), emitterResources_[emitterIndex], emitterData_[emitterIndex], "emitter");
		emitterData_[emitterIndex]->translate = { 0.0f, 0.0f, 0.0f };
		emitterData_[emitterIndex]->radius = 0.15f;
		emitterData_[emitterIndex]->boxSize = { 0.15f, 0.20f, 0.15f };
		emitterData_[emitterIndex]->count = 1;
		emitterData_[emitterIndex]->frequency = 0.15f;
		emitterData_[emitterIndex]->frequencyTime = 0.0f;
		emitterData_[emitterIndex]->emit = 0;
		emitterData_[emitterIndex]->type = static_cast<uint32_t>(EmitterType::Sphere);
		emitterData_[emitterIndex]->particleType = static_cast<uint32_t>(ParticleType::Trail);
	}
	createMappedBuffer(sizeof(PerFrame), perFrameResource_, perFrameData_, "per-frame");
	perFrameData_->time = 0.0f;
	perFrameData_->deltaTime = 0.0f;
	createMappedBuffer(sizeof(Field), fieldResource_, fieldData_, "field");
	fieldData_->acceleration = { 0.0f, -0.5f, 0.0f };
	fieldData_->drag = 0.0f;
	createMappedBuffer(sizeof(DirectionalLight), directionalLightResource_, directionalLightData_, "directional light");
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
	directionalLightData_->intensity = 1.0f;
}

void GPUParticle::SetEmitterTranslate(uint32_t emitterIndex, const Vector3& translate)
{
	if (emitterIndex < kMaxEmitters)
	{
		emitterData_[emitterIndex]->translate = translate;
	}
}

void GPUParticle::SetEmitterEnabled(uint32_t emitterIndex, bool isEnabled)
{
	if (emitterIndex < kMaxEmitters)
	{
		isEmitterEnabled_[emitterIndex] = isEnabled;
	}
}

void GPUParticle::SetEmitterType(uint32_t emitterIndex, EmitterType type)
{
	if (emitterIndex < kMaxEmitters)
	{
		emitterData_[emitterIndex]->type = static_cast<uint32_t>(type);
	}
}

void GPUParticle::SetEmitterParticleType(uint32_t emitterIndex, ParticleType type)
{
	if (emitterIndex < kMaxEmitters)
	{
		emitterData_[emitterIndex]->particleType = static_cast<uint32_t>(type);
	}
}

void GPUParticle::SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity)
{
	directionalLightData_->color = color;
	directionalLightData_->direction = direction;
	directionalLightData_->intensity = intensity;
}

// 時間とEmitter状態を確定してからGPUを更新し、描画で読む状態へ戻します。
void GPUParticle::Update(float deltaTime)
{
	perFrameData_->time += deltaTime;
	perFrameData_->deltaTime = deltaTime;
	UpdateEmitterTiming(deltaTime);
	DispatchParticleSimulation();
}

// 各Emitterの経過時間を進め、今回のフレームで粒子を出すか決めます。
void GPUParticle::UpdateEmitterTiming(float deltaTime)
{
	for (uint32_t emitterIndex = 0; emitterIndex < kMaxEmitters; ++emitterIndex)
	{
		Emitter& emitter = *emitterData_[emitterIndex];
		if (!isEmitterEnabled_[emitterIndex])
		{
			emitter.frequencyTime = 0.0f;
			emitter.emit = 0;
		}
		else if (emitter.frequency <= emitter.frequencyTime + deltaTime) {
			emitter.frequencyTime += deltaTime;
			emitter.frequencyTime -= emitter.frequency;
			emitter.emit = 1;
		} else {
			emitter.frequencyTime += deltaTime;
			emitter.emit = 0;
		}
	}
}

// 発生・更新Computeを実行し、粒子バッファを次の描画で読める状態へ戻します。
void GPUParticle::DispatchParticleSimulation()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	srvManager_->PreDraw();
	D3D12_RESOURCE_BARRIER transitionToUav[3] = {};
	transitionToUav[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionToUav[0].Transition.pResource = particleResource_.Get();
	transitionToUav[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionToUav[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	transitionToUav[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionToUav[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionToUav[1].Transition.pResource = activeParticleIndicesResource_.Get();
	transitionToUav[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionToUav[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	transitionToUav[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionToUav[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionToUav[2].Transition.pResource = drawArgumentsResource_.Get();
	transitionToUav[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionToUav[2].Transition.StateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	transitionToUav[2].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	commandList->ResourceBarrier(_countof(transitionToUav), transitionToUav);

	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterUav, particleUavIndex_);
	bool emitted = false;
	for (uint32_t emitterIndex = 0; emitterIndex < kMaxEmitters; ++emitterIndex)
	{
		Emitter& emitter = *emitterData_[emitterIndex];
		if (emitter.emit == 0) {
			continue;
		}
		commandList->SetPipelineState(emitPipelineState_.Get());
		commandList->SetComputeRootConstantBufferView(kComputeRootParameterEmitter, emitterResources_[emitterIndex]->GetGPUVirtualAddress());
		commandList->SetComputeRootConstantBufferView(kComputeRootParameterPerFrame, perFrameResource_->GetGPUVirtualAddress());
		commandList->Dispatch((emitter.count + 63) / 64, 1, 1);
		emitted = true;
	}

	if (emitted) {
		D3D12_RESOURCE_BARRIER emitUavBarriers[3] = {};
		emitUavBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		emitUavBarriers[0].UAV.pResource = particleResource_.Get();
		emitUavBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		emitUavBarriers[1].UAV.pResource = freeListIndexResource_.Get();
		emitUavBarriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		emitUavBarriers[2].UAV.pResource = freeListResource_.Get();
		commandList->ResourceBarrier(_countof(emitUavBarriers), emitUavBarriers);
	}

	commandList->SetPipelineState(updatePipelineState_.Get());
	commandList->SetComputeRootConstantBufferView(kComputeRootParameterPerFrame, perFrameResource_->GetGPUVirtualAddress());
	commandList->SetComputeRootConstantBufferView(kComputeRootParameterField, fieldResource_->GetGPUVirtualAddress());
	commandList->Dispatch(1, 1, 1);

	D3D12_RESOURCE_BARRIER updateUavBarriers[5] = {};
	updateUavBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	updateUavBarriers[0].UAV.pResource = particleResource_.Get();
	updateUavBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	updateUavBarriers[1].UAV.pResource = freeListIndexResource_.Get();
	updateUavBarriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	updateUavBarriers[2].UAV.pResource = freeListResource_.Get();
	updateUavBarriers[3].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	updateUavBarriers[3].UAV.pResource = activeParticleIndicesResource_.Get();
	updateUavBarriers[4].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	updateUavBarriers[4].UAV.pResource = drawArgumentsResource_.Get();
	commandList->ResourceBarrier(_countof(updateUavBarriers), updateUavBarriers);

	D3D12_RESOURCE_BARRIER transitionForDraw[3] = {};
	transitionForDraw[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionForDraw[0].Transition.pResource = particleResource_.Get();
	transitionForDraw[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionForDraw[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionForDraw[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	transitionForDraw[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionForDraw[1].Transition.pResource = activeParticleIndicesResource_.Get();
	transitionForDraw[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionForDraw[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionForDraw[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	transitionForDraw[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionForDraw[2].Transition.pResource = drawArgumentsResource_.Get();
	transitionForDraw[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionForDraw[2].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionForDraw[2].Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	commandList->ResourceBarrier(_countof(transitionForDraw), transitionForDraw);
}

// Cameraと固定画像の両方が準備済みの時だけ、Compute結果を画面へ描きます。
void GPUParticle::Draw(const Camera* camera)
{
	if (!camera || !isTextureReady_) {
		return;
	}

	perViewData_->viewProjectionMatrix = camera->GetViewProjectionMatrix();
	Matrix4x4 viewMatrix = camera->GetViewMatrix();
	viewMatrix.m[3][0] = 0.0f;
	viewMatrix.m[3][1] = 0.0f;
	viewMatrix.m[3][2] = 0.0f;
	perViewData_->billboardMatrix = Inverse(viewMatrix);

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(graphicsRootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(kGraphicsRootParameterPerView, perViewResource_->GetGPUVirtualAddress());
	srvManager_->SetGraphicsRootDescriptorTable(kGraphicsRootParameterParticle, particleSrvIndex_);
	commandList->SetGraphicsRootDescriptorTable(kGraphicsRootParameterTexture, TextureManager::GetInstance()->GetSrvHandleGPU(kParticleTexturePath));
	commandList->SetGraphicsRootConstantBufferView(kGraphicsRootParameterDirectionalLight, directionalLightResource_->GetGPUVirtualAddress());
	commandList->ExecuteIndirect(drawCommandSignature_.Get(), 1, drawArgumentsResource_.Get(), 0, nullptr, 0);
}

// Textureの解放はTextureManagerに任せ、Draw用の準備状態と自分のGPU資源を片付けます。
void GPUParticle::Finalize()
{
	if (srvManager_) {
		srvManager_->Free(particleSrvIndex_);
		srvManager_->Free(activeParticleIndicesSrvIndex_);
		srvManager_->Free(particleUavIndex_);
		srvManager_->Free(freeListIndexUavIndex_);
		srvManager_->Free(freeListUavIndex_);
		srvManager_->Free(activeParticleIndicesUavIndex_);
		srvManager_->Free(drawArgumentsUavIndex_);
	}
	particleSrvIndex_ = SrvManager::kInvalidSrvIndex;
	activeParticleIndicesSrvIndex_ = SrvManager::kInvalidSrvIndex;
	particleUavIndex_ = SrvManager::kInvalidSrvIndex;
	freeListIndexUavIndex_ = SrvManager::kInvalidSrvIndex;
	freeListUavIndex_ = SrvManager::kInvalidSrvIndex;
	activeParticleIndicesUavIndex_ = SrvManager::kInvalidSrvIndex;
	drawArgumentsUavIndex_ = SrvManager::kInvalidSrvIndex;
	particleResource_.Reset();
	freeListIndexResource_.Reset();
	freeListResource_.Reset();
	activeParticleIndicesResource_.Reset();
	drawArgumentsResource_.Reset();
	vertexResource_.Reset();
	perViewResource_.Reset();
	for (Microsoft::WRL::ComPtr<ID3D12Resource>& emitterResource : emitterResources_)
	{
		emitterResource.Reset();
	}
	perFrameResource_.Reset();
	fieldResource_.Reset();
	directionalLightResource_.Reset();
	computeRootSignature_.Reset();
	initializePipelineState_.Reset();
	emitPipelineState_.Reset();
	updatePipelineState_.Reset();
	graphicsRootSignature_.Reset();
	graphicsPipelineState_.Reset();
	drawCommandSignature_.Reset();
	perViewData_ = nullptr;
	emitterData_.fill(nullptr);
	perFrameData_ = nullptr;
	fieldData_ = nullptr;
	directionalLightData_ = nullptr;
	vertexBufferView_ = {};
	isEmitterEnabled_.fill(false);
	dxCommon_ = nullptr;
	srvManager_ = nullptr;
	isTextureReady_ = false;
}

// Compute用のRoot Signatureと三段階のPipelineを作り、未完成ならGPU更新を始めません。
void GPUParticle::CreateComputePipeline()
{
	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.BaseShaderRegister = 0;
	descriptorRange.NumDescriptors = 5;
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[kComputeRootParameterCount] = {};
	rootParameters[kComputeRootParameterUav].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[kComputeRootParameterUav].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kComputeRootParameterUav].DescriptorTable.pDescriptorRanges = &descriptorRange;
	rootParameters[kComputeRootParameterUav].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[kComputeRootParameterEmitter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[kComputeRootParameterEmitter].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kComputeRootParameterEmitter].Descriptor.ShaderRegister = 0;
	rootParameters[kComputeRootParameterPerFrame].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[kComputeRootParameterPerFrame].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kComputeRootParameterPerFrame].Descriptor.ShaderRegister = 1;
	rootParameters[kComputeRootParameterField].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[kComputeRootParameterField].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kComputeRootParameterField].Descriptor.ShaderRegister = 2;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr) || !signatureBlob) {
		throw std::runtime_error("GPUParticle: compute root signature serialization failed");
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature_));
	if (FAILED(hr) || !computeRootSignature_) {
		throw std::runtime_error("GPUParticle: compute root signature creation failed");
	}

	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(L"resources/shaders/InitializeParticle.CS.hlsl", L"cs_6_0");
	D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc{};
	pipelineStateDesc.pRootSignature = computeRootSignature_.Get();
	pipelineStateDesc.CS = { computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize() };
	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&initializePipelineState_));
	if (FAILED(hr) || !initializePipelineState_) {
		throw std::runtime_error("GPUParticle: initialization compute pipeline creation failed");
	}

	Microsoft::WRL::ComPtr<IDxcBlob> emitShaderBlob = dxCommon_->CompileShader(L"resources/shaders/EmitParticle.CS.hlsl", L"cs_6_0");
	pipelineStateDesc.CS = { emitShaderBlob->GetBufferPointer(), emitShaderBlob->GetBufferSize() };
	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&emitPipelineState_));
	if (FAILED(hr) || !emitPipelineState_) {
		throw std::runtime_error("GPUParticle: emission compute pipeline creation failed");
	}

	Microsoft::WRL::ComPtr<IDxcBlob> updateShaderBlob = dxCommon_->CompileShader(L"resources/shaders/UpdateParticle.CS.hlsl", L"cs_6_0");
	pipelineStateDesc.CS = { updateShaderBlob->GetBufferPointer(), updateShaderBlob->GetBufferSize() };
	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&pipelineStateDesc, IID_PPV_ARGS(&updatePipelineState_));
	if (FAILED(hr) || !updatePipelineState_) {
		throw std::runtime_error("GPUParticle: update compute pipeline creation failed");
	}
}

// 頂点・画像の結合と間接Drawの設定を作り、どちらかが欠けた時は描画を始めません。
void GPUParticle::CreateGraphicsPipeline()
{
	D3D12_DESCRIPTOR_RANGE particleDescriptorRange{};
	particleDescriptorRange.BaseShaderRegister = 0;
	particleDescriptorRange.NumDescriptors = 2;
	particleDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	particleDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	D3D12_DESCRIPTOR_RANGE textureDescriptorRange{};
	textureDescriptorRange.BaseShaderRegister = 0;
	textureDescriptorRange.NumDescriptors = 1;
	textureDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	textureDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[kGraphicsRootParameterCount] = {};
	rootParameters[kGraphicsRootParameterPerView].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[kGraphicsRootParameterPerView].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[kGraphicsRootParameterPerView].Descriptor.ShaderRegister = 0;
	rootParameters[kGraphicsRootParameterParticle].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[kGraphicsRootParameterParticle].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[kGraphicsRootParameterParticle].DescriptorTable.pDescriptorRanges = &particleDescriptorRange;
	rootParameters[kGraphicsRootParameterParticle].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[kGraphicsRootParameterTexture].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[kGraphicsRootParameterTexture].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[kGraphicsRootParameterTexture].DescriptorTable.pDescriptorRanges = &textureDescriptorRange;
	rootParameters[kGraphicsRootParameterTexture].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[kGraphicsRootParameterDirectionalLight].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[kGraphicsRootParameterDirectionalLight].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[kGraphicsRootParameterDirectionalLight].Descriptor.ShaderRegister = 1;

	D3D12_STATIC_SAMPLER_DESC staticSampler{};
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
	staticSampler.ShaderRegister = 0;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pStaticSamplers = &staticSampler;
	rootSignatureDesc.NumStaticSamplers = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr) || !signatureBlob) {
		throw std::runtime_error("GPUParticle: graphics root signature serialization failed");
	}
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&graphicsRootSignature_));
	if (FAILED(hr) || !graphicsRootSignature_) {
		throw std::runtime_error("GPUParticle: graphics root signature creation failed");
	}

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/GPUParticle.VS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/GPUParticle.PS.hlsl", L"ps_6_0");
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
	pipelineStateDesc.pRootSignature = graphicsRootSignature_.Get();
	pipelineStateDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	pipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	pipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	pipelineStateDesc.BlendState = blendDesc;
	pipelineStateDesc.RasterizerState = rasterizerDesc;
	pipelineStateDesc.DepthStencilState = depthStencilDesc;
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateDesc.SampleDesc.Count = 1;
	pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	if (FAILED(hr) || !graphicsPipelineState_) {
		throw std::runtime_error("GPUParticle: graphics pipeline creation failed");
	}

	D3D12_INDIRECT_ARGUMENT_DESC indirectArgumentDesc{};
	indirectArgumentDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc{};
	commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
	commandSignatureDesc.NumArgumentDescs = 1;
	commandSignatureDesc.pArgumentDescs = &indirectArgumentDesc;
	hr = dxCommon_->GetDevice()->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(&drawCommandSignature_));
	if (FAILED(hr) || !drawCommandSignature_) {
		throw std::runtime_error("GPUParticle: indirect draw command signature creation failed");
	}
}

// 全資源の準備後だけCompute命令を送り、初期粒子と空き枠をGPU上に作ります。
void GPUParticle::InitializeParticles()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	if (!commandList) {
		throw std::runtime_error("GPUParticle: command list is unavailable during initialization");
	}
	srvManager_->PreDraw();
	commandList->SetComputeRootSignature(computeRootSignature_.Get());
	commandList->SetPipelineState(initializePipelineState_.Get());
	srvManager_->SetComputeRootDescriptorTable(kComputeRootParameterUav, particleUavIndex_);
	commandList->Dispatch(1, 1, 1);

	D3D12_RESOURCE_BARRIER uavBarriers[3] = {};
	uavBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarriers[0].UAV.pResource = particleResource_.Get();
	uavBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarriers[1].UAV.pResource = freeListIndexResource_.Get();
	uavBarriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarriers[2].UAV.pResource = freeListResource_.Get();
	commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);

	D3D12_RESOURCE_BARRIER transitionForDraw[3] = {};
	transitionForDraw[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionForDraw[0].Transition.pResource = particleResource_.Get();
	transitionForDraw[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionForDraw[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionForDraw[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	transitionForDraw[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionForDraw[1].Transition.pResource = activeParticleIndicesResource_.Get();
	transitionForDraw[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionForDraw[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionForDraw[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	transitionForDraw[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	transitionForDraw[2].Transition.pResource = drawArgumentsResource_.Get();
	transitionForDraw[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	transitionForDraw[2].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	transitionForDraw[2].Transition.StateAfter = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	commandList->ResourceBarrier(_countof(transitionForDraw), transitionForDraw);
}
