#include "ParticleManager.h"
#include "TextureManager.h"
#include "CameraManager.h"
#include <algorithm>

using namespace MyMath;

// シングルトンインスタンスの取得
ParticleManager* ParticleManager::GetInstance() {
    static ParticleManager instance;
    return &instance;
}

// 初期化処理
void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    // 引数でポインタを受け取ってメンバ変数に記録
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    // ランダムエンジンの初期化
    std::random_device seed_gen;
    randomEngine_.seed(seed_gen());

    ID3D12Device* device = dxCommon_->GetDevice();
    HRESULT hr = S_FALSE;

    D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1] = {};
    descriptorRangeForInstancing[0].BaseShaderRegister = 0;
    descriptorRangeForInstancing[0].NumDescriptors = 1;
    descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    assert(SUCCEEDED(hr));
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;

    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    assert(SUCCEEDED(hr));

    //頂点データの初期化
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * 6);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    Vector3 normal = { 0.0f, 0.0f, -1.0f };
    vertexData[0] = { {-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, normal };
    vertexData[1] = { {-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, normal };
    vertexData[2] = { { 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, normal };
    vertexData[3] = { {-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, normal };
    vertexData[4] = { { 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, normal };
    vertexData[5] = { { 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, normal };

    // マテリアル（定数バッファ）の初期化
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    memset(materialData_, 0, sizeof(Material));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 0;
    materialData_->uvTransform = MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });

    //フィールドの設定
    accelerationField_.acceleration = { 15.0f,0.0f,0.0f };
    accelerationField_.area.min = { -1.0f,-1.0f,-1.0f };
    accelerationField_.area.max = { 1.0f,1.0f,1.0f };

    isField_ = true;
	returnState_.Reset();
}

// パーティクルグループの生成
void ParticleManager::CreateParticleGroup(const std::string name, const std::string textureFilePath) {
	if (!srvManager_ || !srvManager_->CanAllocate()) {
		return;
	}
    if (auto it = particleGroups_.find(name); it != particleGroups_.end()) {
        if (srvManager_) {
            srvManager_->Free(it->second.instancingSrvIndex);
        }
        particleGroups_.erase(it);
    }
    // 登録済みの名前かチェックしてassert
    assert(particleGroups_.find(name) == particleGroups_.end() && "その名前のグループは既に存在します");

    // 新たな空のパーティクルグループを作成
    ParticleGroup newGroup{};
    // マテリアルデータにテクスチャファイルパスを設定
    newGroup.textureFilePath = textureFilePath;

    // インスタンシング用リソースの生成 (上限を決めてリソースを作る)
    newGroup.instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);

    // ポインタを取得
    newGroup.instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.mappedData));

    // インスタンシング用にSRVを確保してSRVインデックスを記録
    newGroup.instancingSrvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(
        newGroup.instancingSrvIndex,
        newGroup.instancingResource.Get(),
        kNumMaxInstance,
        sizeof(ParticleForGPU)
    );

    // コンテナに登録
    particleGroups_[name] = newGroup;
}

void ParticleManager::CreateRingParticleGroup(const std::string name, const std::string textureFilePath)
{
	if (!srvManager_ || !srvManager_->CanAllocate()) {
		return;
	}
    if (auto it = particleGroups_.find(name); it != particleGroups_.end()) {
        if (srvManager_) {
            srvManager_->Free(it->second.instancingSrvIndex);
        }
        particleGroups_.erase(it);
    }
    assert(particleGroups_.find(name) == particleGroups_.end() && "Particle group already exists");

    ParticleGroup newGroup{};
    newGroup.textureFilePath = textureFilePath;
    newGroup.useBillboard = false;

    newGroup.instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);
    newGroup.instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.mappedData));

    newGroup.instancingSrvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(
        newGroup.instancingSrvIndex,
        newGroup.instancingResource.Get(),
        kNumMaxInstance,
        sizeof(ParticleForGPU)
    );

    const uint32_t kRingDivide = 32;
    const float kOuterRadius = 1.0f;
    const float kInnerRadius = 0.6f;
    const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(kRingDivide);
    const Vector3 normal = { 0.0f, 0.0f, -1.0f };

    newGroup.vertexCount = kRingDivide * 6;
    newGroup.vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * newGroup.vertexCount);
    newGroup.vertexBufferView.BufferLocation = newGroup.vertexResource->GetGPUVirtualAddress();
    newGroup.vertexBufferView.SizeInBytes = sizeof(VertexData) * newGroup.vertexCount;
    newGroup.vertexBufferView.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    newGroup.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

    for (uint32_t index = 0; index < kRingDivide; ++index) {
        float sin = std::sin(index * radianPerDivide);
        float cos = std::cos(index * radianPerDivide);
        float sinNext = std::sin((index + 1) * radianPerDivide);
        float cosNext = std::cos((index + 1) * radianPerDivide);
        float u = float(index) / float(kRingDivide);
        float uNext = float(index + 1) / float(kRingDivide);

        uint32_t vertexIndex = index * 6;
        vertexData[vertexIndex + 0] = { { -sin * kOuterRadius, cos * kOuterRadius, 0.0f, 1.0f }, { u, 0.0f }, normal };
        vertexData[vertexIndex + 1] = { { -sinNext * kOuterRadius, cosNext * kOuterRadius, 0.0f, 1.0f }, { uNext, 0.0f }, normal };
        vertexData[vertexIndex + 2] = { { -sin * kInnerRadius, cos * kInnerRadius, 0.0f, 1.0f }, { u, 1.0f }, normal };
        vertexData[vertexIndex + 3] = { { -sinNext * kOuterRadius, cosNext * kOuterRadius, 0.0f, 1.0f }, { uNext, 0.0f }, normal };
        vertexData[vertexIndex + 4] = { { -sinNext * kInnerRadius, cosNext * kInnerRadius, 0.0f, 1.0f }, { uNext, 1.0f }, normal };
        vertexData[vertexIndex + 5] = { { -sin * kInnerRadius, cos * kInnerRadius, 0.0f, 1.0f }, { u, 1.0f }, normal };
    }

    particleGroups_[name] = newGroup;
}

void ParticleManager::CreateCylinderParticleGroup(
    const std::string name,
    const std::string textureFilePath,
    uint32_t divide,
    float topRadius,
    float bottomRadius,
    float height)
{
	if (!srvManager_ || !srvManager_->CanAllocate()) {
		return;
	}
    if (auto it = particleGroups_.find(name); it != particleGroups_.end()) {
        if (srvManager_) {
            srvManager_->Free(it->second.instancingSrvIndex);
        }
        particleGroups_.erase(it);
    }
    assert(particleGroups_.find(name) == particleGroups_.end() && "Particle group already exists");
    assert(divide >= 3);
    assert(topRadius > 0.0f);
    assert(bottomRadius > 0.0f);
    assert(height > 0.0f);

    ParticleGroup newGroup{};
    newGroup.textureFilePath = textureFilePath;
    newGroup.useBillboard = false;
	newGroup.updateCallback = UpdateCylinderParticle;

    newGroup.instancingResource = dxCommon_->CreateBufferResource(sizeof(ParticleForGPU) * kNumMaxInstance);
    newGroup.instancingResource->Map(0, nullptr, reinterpret_cast<void**>(&newGroup.mappedData));

    newGroup.instancingSrvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(
        newGroup.instancingSrvIndex,
        newGroup.instancingResource.Get(),
        kNumMaxInstance,
        sizeof(ParticleForGPU)
    );

    const uint32_t verticesPerDivide = 6;
    const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(divide);

    newGroup.vertexCount = divide * verticesPerDivide;
    newGroup.vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * newGroup.vertexCount);
    newGroup.vertexBufferView.BufferLocation = newGroup.vertexResource->GetGPUVirtualAddress();
    newGroup.vertexBufferView.SizeInBytes = sizeof(VertexData) * newGroup.vertexCount;
    newGroup.vertexBufferView.StrideInBytes = sizeof(VertexData);

    VertexData* vertexData = nullptr;
    newGroup.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

    for (uint32_t index = 0; index < divide; ++index) {
        float sin = std::sin(index * radianPerDivide);
        float cos = std::cos(index * radianPerDivide);
        float sinNext = std::sin((index + 1) * radianPerDivide);
        float cosNext = std::cos((index + 1) * radianPerDivide);
        float u = float(index) / float(divide);
        float uNext = float(index + 1) / float(divide);

        Vector3 normal0 = { -sin, 0.0f, cos };
        Vector3 normal1 = { -sinNext, 0.0f, cosNext };

        uint32_t vertexIndex = index * verticesPerDivide;
        vertexData[vertexIndex + 0] = { { -sin * topRadius, height, cos * topRadius, 1.0f }, { u, 1.0f }, normal0 };
        vertexData[vertexIndex + 1] = { { -sinNext * topRadius, height, cosNext * topRadius, 1.0f }, { uNext, 1.0f }, normal1 };
        vertexData[vertexIndex + 2] = { { -sin * bottomRadius, 0.0f, cos * bottomRadius, 1.0f }, { u, 0.0f }, normal0 };
        vertexData[vertexIndex + 3] = { { -sinNext * topRadius, height, cosNext * topRadius, 1.0f }, { uNext, 1.0f }, normal1 };
        vertexData[vertexIndex + 4] = { { -sinNext * bottomRadius, 0.0f, cosNext * bottomRadius, 1.0f }, { uNext, 0.0f }, normal1 };
        vertexData[vertexIndex + 5] = { { -sin * bottomRadius, 0.0f, cos * bottomRadius, 1.0f }, { u, 0.0f }, normal0 };
    }

    particleGroups_[name] = newGroup;
}

void ParticleManager::ClearAllParticles()
{
    for (auto& pair : particleGroups_) {
        pair.second.particles.clear();
        pair.second.instanceCount = 0; // インスタンス数もリセット
    }
}

void ParticleManager::ClearAllGroups()
{
    if (srvManager_) {
        for (auto& [name, group] : particleGroups_) {
            srvManager_->Free(group.instancingSrvIndex);
        }
    }
    particleGroups_.clear();
	returnState_.Reset();
}

void ParticleManager::ClearParticles(const std::string name)
{
    auto it = particleGroups_.find(name);
    if (it == particleGroups_.end()) {
        return;
    }
    it->second.particles.clear();
    it->second.instanceCount = 0;
}

bool ParticleManager::GetPlaybackSnapshot(const std::string& name, ParticlePlaybackSnapshot& snapshot) const
{
    snapshot = {};
    const auto groupIt = particleGroups_.find(name);
    if (groupIt == particleGroups_.end()) {
        return false;
    }

    snapshot.count = groupIt->second.particles.size();
    if (groupIt->second.particles.empty()) {
        return true;
    }

    const Particle& particle = groupIt->second.particles.front();
    snapshot.transform = particle.transform;
    snapshot.velocity = particle.velocity;
    snapshot.currentTime = particle.currentTime;
    snapshot.lifeTime = particle.lifeTime;
    snapshot.isEndless = particle.isEndless;
    return true;
}

size_t ParticleManager::GetTotalParticleCount() const
{
    size_t count = 0;
    for (const auto& [name, group] : particleGroups_) {
        (void)name;
        count += group.particles.size();
    }
    return count;
}

bool ParticleManager::GetBillboardEnabled(const std::string& name) const
{
    auto it = particleGroups_.find(name);
    return it != particleGroups_.end() ? it->second.useBillboard : false;
}

void ParticleManager::SetBillboardEnabled(const std::string& name, bool isEnabled)
{
    auto it = particleGroups_.find(name);
    if (it == particleGroups_.end()) {
        return;
    }
    it->second.useBillboard = isEnabled;
}

bool ParticleManager::IsCollision(const AABB aabb, const Vector3& point)
{
    return (point.x >= aabb.min.x && point.x <= aabb.max.x) &&
        (point.y >= aabb.min.y && point.y <= aabb.max.y) &&
        (point.z >= aabb.min.z && point.z <= aabb.max.z);
}

void ParticleManager::UpdateCylinderParticle(Particle& particle, float signedDeltaTime, bool returning)
{
    particle.transform.rotate.y += 1.5f * signedDeltaTime;

    // 逆再生時は、境界で反転した速度を先に戻してからスケールを巻き戻す。
    if (returning) {
        if (particle.transform.scale.y >= 1.2f && particle.scaleVelocityY < 0.0f) {
            particle.scaleVelocityY *= -1.0f;
        } else if (particle.transform.scale.y <= 0.5f && particle.scaleVelocityY > 0.0f) {
            particle.scaleVelocityY *= -1.0f;
        }
    }

    particle.transform.scale.y += particle.scaleVelocityY * signedDeltaTime;
    if (particle.transform.scale.y >= 1.2f) {
        particle.transform.scale.y = 1.2f;
        particle.scaleVelocityY *= -1.0f;
    } else if (particle.transform.scale.y <= 0.5f) {
        particle.transform.scale.y = 0.5f;
        particle.scaleVelocityY *= -1.0f;
    }
}

// パーティクルの発生
void ParticleManager::Emit(const std::string name, const Vector3& position, uint32_t count,bool receivesWind, float scaleMultiplier) {
	if (returnState_.IsReturning()) return;
    auto groupIt = particleGroups_.find(name);
    if (groupIt == particleGroups_.end()) {
        return;
    }
    // 指定されたグループの参照を取得
    ParticleGroup& group = groupIt->second;

    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    std::uniform_real_distribution<float> distColor(0.0f, 1.0f);
    std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

    for (uint32_t i = 0; i < count; ++i) {
        Particle newParticle;
        newParticle.transform.scale = { scaleMultiplier, scaleMultiplier, scaleMultiplier };
        newParticle.transform.rotate = { 0.0f, 0.0f, 0.0f };
        newParticle.transform.translate = position;

        newParticle.velocity = {
            distribution(randomEngine_),
            distribution(randomEngine_),
            distribution(randomEngine_)
        };
        newParticle.color = {
            distColor(randomEngine_), distColor(randomEngine_), distColor(randomEngine_), 1.0f
        };
        newParticle.lifeTime = distTime(randomEngine_);
        newParticle.currentTime = 0.0f;

        newParticle.receivesWind = receivesWind;
        // グループに登録
        group.particles.push_back(newParticle);
    }
}

//ヒット(斬撃みたいな細長い円)エフェクト発生
void ParticleManager::Update() {

    if (!cameraManager_)return;
    //毎フレーム今アクティブなカメラを取得する
    Camera* activeCamera = cameraManager_->GetActiveCamera();
    //万が一カメラがない場合は安全のために抜ける
    if (!activeCamera)return;

    const bool returning = returnState_.IsReturning();
    const float signedDeltaTime = dxCommon_->GetDeltaTime() * (returning ? -1.0f : 1.0f);

    //風タイマー
    if (autoWindSwitch_) {
        windTimer_ += signedDeltaTime;
    }
    if (autoWindSwitch_ && returning && windTimer_ < 0.0f) {
        isField_ = !isField_;
        windTimer_ += 5.0f;
    }
    else if (autoWindSwitch_ && windTimer_ >= 5.0f)
    {//5秒経過したら
        isField_ = !isField_;//trueとfalse切り替え
        windTimer_ = 0.0f;
    }

    //取得したactiveカメラからビュー作成もらう
    const Matrix4x4 viewProjectionMatrix = Multiply(activeCamera->GetViewMatrix(), activeCamera->GetProjectionMatrix());
    Matrix4x4 view = activeCamera->GetViewMatrix();
    view.m[3][0] = 0.0f; view.m[3][1] = 0.0f; view.m[3][2] = 0.0f;
    Matrix4x4 billboardMatrix = Inverse(view);

    // 全てのパーティクルグループについて処理する
    for (auto& pair : particleGroups_) {
        ParticleGroup& group = pair.second;
        // インスタンス数をリセット
        group.instanceCount = 0;

        // グループ内の全てのパーティクルについて処理する
        for (auto it = group.particles.begin(); it != group.particles.end();) {
            Particle& particle = *it;

            if (!returning && !particle.isEndless && particle.currentTime >= particle.lifeTime) {
                it = group.particles.erase(it);
                continue;
            }

            if (group.updateCallback) {
                group.updateCallback(particle, signedDeltaTime, returning);
            }

            if (particle.isSpiral) {
                particle.spiralAngle += particle.spiralAngularVelocity * signedDeltaTime;
                particle.spiralRadius = (std::max)(0.05f, particle.spiralRadius + particle.spiralRadialVelocity * signedDeltaTime);

                const float spiralX = std::cos(particle.spiralAngle) * particle.spiralRadius;
                const float spiralY = std::sin(particle.spiralAngle) * particle.spiralRadius;
                particle.transform.translate = {
                    particle.spiralCenter.x + particle.spiralRight.x * spiralX + particle.spiralUp.x * spiralY,
                    particle.spiralCenter.y + particle.spiralRight.y * spiralX + particle.spiralUp.y * spiralY,
                    particle.spiralCenter.z + particle.spiralRight.z * spiralX + particle.spiralUp.z * spiralY
                };
            }

            // Forward Euler first changes velocity and then position. Its inverse
            // must restore position before restoring velocity to avoid drift.
            if (returning) {
                particle.transform.translate.x += particle.velocity.x * signedDeltaTime;
                particle.transform.translate.y += particle.velocity.y * signedDeltaTime;
                particle.transform.translate.z += particle.velocity.z * signedDeltaTime;
            }

            const bool receivesActiveWind = isField_ && particle.receivesWind &&
                IsCollision(accelerationField_.area, particle.transform.translate);
            if (receivesActiveWind)
            {
                particle.velocity.x += accelerationField_.acceleration.x * signedDeltaTime;
                particle.velocity.y += accelerationField_.acceleration.y * signedDeltaTime;
                particle.velocity.z += accelerationField_.acceleration.z * signedDeltaTime;
            }

            if (!returning) {
                particle.transform.translate.x += particle.velocity.x * signedDeltaTime;
                particle.transform.translate.y += particle.velocity.y * signedDeltaTime;
                particle.transform.translate.z += particle.velocity.z * signedDeltaTime;
            }

            float alpha = particle.color.w;
            if (!particle.isEndless) {
                particle.currentTime += signedDeltaTime;
                if (returning && particle.lifeTime > 0.0f) {
                    while (particle.currentTime < 0.0f) {
                        particle.currentTime += particle.lifeTime;
                    }
                }
                const float progress = std::clamp(particle.currentTime / particle.lifeTime, 0.0f, 1.0f);
                if (particle.useColorAndScaleOverLife) {
                    particle.transform.scale = {
                        particle.startScale.x + (particle.endScale.x - particle.startScale.x) * progress,
                        particle.startScale.y + (particle.endScale.y - particle.startScale.y) * progress,
                        particle.startScale.z + (particle.endScale.z - particle.startScale.z) * progress
                    };
                    particle.color = {
                        particle.startColor.x + (particle.endColor.x - particle.startColor.x) * progress,
                        particle.startColor.y + (particle.endColor.y - particle.startColor.y) * progress,
                        particle.startColor.z + (particle.endColor.z - particle.startColor.z) * progress,
                        particle.startColor.w + (particle.endColor.w - particle.startColor.w) * progress
                    };
                    alpha = particle.color.w;
                } else {
                    alpha = particle.color.w * (1.0f - progress);
                }
            }

            Matrix4x4 scale = MakeScaleMatrix(particle.transform.scale);
            Matrix4x4 rotateZ = MakeRotateZMatrix(particle.transform.rotate.z);
            Matrix4x4 translate = MakeTranslateMatrix(particle.transform.translate);

            Matrix4x4 rotateY = MakeRotateYMatrix(particle.transform.rotate.y);
            Matrix4x4 rotateX = MakeRotateXMatrix(particle.transform.rotate.x);
            Matrix4x4 rotate = Multiply(Multiply(rotateX, rotateY), rotateZ);
            Matrix4x4 rotateOrBillboard = group.useBillboard ? Multiply(rotateZ, billboardMatrix) : rotate;
            Matrix4x4 worldMatrix = Multiply(Multiply(scale, rotateOrBillboard), translate);

            // 取得したactiveCameraからビュープロジェクション行列をもらう(カメラの向きを向く)
            if (group.instanceCount < kNumMaxInstance) {
                group.mappedData[group.instanceCount].WVP = viewProjectionMatrix;
                group.mappedData[group.instanceCount].World = worldMatrix;
                group.mappedData[group.instanceCount].color = particle.color;
                group.mappedData[group.instanceCount].color.w = alpha;
                group.instanceCount++;
            }
            ++it;
        }
    }
}

// 描画処理
void ParticleManager::Draw() {

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // 1〜4. パイプラインと頂点データの設定
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(graphicsPipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // マテリアル定数バッファ
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    for (auto& pair : particleGroups_) {
        ParticleGroup& group = pair.second;

        if (group.instanceCount == 0) continue;

        D3D12_VERTEX_BUFFER_VIEW vertexBufferView = group.vertexResource ? group.vertexBufferView : vertexBufferView_;
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

        // 5. インスタンシング用 SRV
        srvManager_->SetGraphicsRootDescriptorTable(1, group.instancingSrvIndex);

        // 6. テクスチャ用 SRV
        D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = TextureManager::GetInstance()->GetSrvHandleGPU(group.textureFilePath);
        commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

        // 7. インスタンシング描画
        commandList->DrawInstanced(group.vertexCount, group.instanceCount, 0, 0);
    }
}
