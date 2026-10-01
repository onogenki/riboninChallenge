#include "PostEffect.h"

#include "TextureManager.h"

#include <cassert>

void PostEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	assert(dxCommon);
	assert(srvManager);
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	CreateRootSignature();
	CreateGraphicsPipeline();

	depthBasedOutlineResource_ = dxCommon_->CreateBufferResource(sizeof(DepthBasedOutlineData));
	depthBasedOutlineResource_->Map(0, nullptr, reinterpret_cast<void**>(&depthBasedOutlineData_));
	dissolveResource_ = dxCommon_->CreateBufferResource(sizeof(DissolveData));
	dissolveResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveData_));
	dissolveData_->threshold = 0.5f;
	dissolveData_->edgeWidth = 0.03f;
	dissolveData_->edgeColor[0] = 1.0f;
	dissolveData_->edgeColor[1] = 0.4f;
	dissolveData_->edgeColor[2] = 0.3f;
	dissolveData_->edgeColor[3] = 1.0f;
	randomNoiseResource_ = dxCommon_->CreateBufferResource(sizeof(RandomNoiseData));
	randomNoiseResource_->Map(0, nullptr, reinterpret_cast<void**>(&randomNoiseData_));
	randomNoiseData_->time = 0.0f;
	randomNoiseData_->intensity = 1.0f;

	const std::string dissolveMaskFilePath = "Resources/debug/noise0.png";
	const bool isLoaded = TextureManager::GetInstance()->LoadTexture(dissolveMaskFilePath);
	assert(isLoaded);
	dissolveMaskSrvIndex_ = TextureManager::GetInstance()->GetSrvIndex(dissolveMaskFilePath);

	const std::string dissolveMaskNoise1FilePath = "Resources/debug/noise1.png";
	const bool isNoise1Loaded = TextureManager::GetInstance()->LoadTexture(dissolveMaskNoise1FilePath);
	assert(isNoise1Loaded);
	dissolveMaskNoise1SrvIndex_ = TextureManager::GetInstance()->GetSrvIndex(dissolveMaskNoise1FilePath);
}

void PostEffect::SetDissolveThreshold(float threshold)
{
	dissolveData_->threshold = threshold;
}

void PostEffect::Update(float deltaTime)
{
	randomNoiseData_->time += deltaTime;
}

void PostEffect::SetRandomNoiseIntensity(float intensity)
{
	randomNoiseData_->intensity = intensity;
}

void PostEffect::SetDissolveEdgeWidth(float edgeWidth)
{
	dissolveData_->edgeWidth = edgeWidth;
}

void PostEffect::SetDissolveEdgeColor(float red, float green, float blue)
{
	dissolveData_->edgeColor[0] = red;
	dissolveData_->edgeColor[1] = green;
	dissolveData_->edgeColor[2] = blue;
}

void PostEffect::GetDissolveEdgeColor(float& red, float& green, float& blue) const
{
	red = dissolveData_->edgeColor[0];
	green = dissolveData_->edgeColor[1];
	blue = dissolveData_->edgeColor[2];
}

void PostEffect::SetProjectionInverse(const Matrix4x4& projectionInverse)
{
	depthBasedOutlineData_->projectionInverse = projectionInverse;
}

void PostEffect::Draw()
{
	Draw(dxCommon_->GetRenderTextureSrvIndex(), true);
}

void PostEffect::Draw(uint32_t sourceSrvIndex, bool useEffect)
{
	const PipelineType pipelineType = !useEffect
		? PipelineType::Fullscreen
		: isDepthBasedOutline_
		? PipelineType::DepthBasedOutline
		: isLuminanceBasedOutline_
		? PipelineType::LuminanceBasedOutline
		: isGaussianFilter_
		? PipelineType::GaussianFilterHorizontal
		: isRadialBlur_
		? PipelineType::RadialBlur
		: isDissolve_
		? (isDissolveEdge_ ? PipelineType::DissolveEdge : PipelineType::Dissolve)
		: isRandomNoise_
		? PipelineType::RandomNoise
		: isSmoothing_
		? PipelineType::Smoothing
		: isVignette_
		? PipelineType::Vignette
		: isSepia_
		? PipelineType::Sepia
		: (isGrayscale_ ? PipelineType::Grayscale : PipelineType::Fullscreen);
	DrawWithPipeline(sourceSrvIndex, pipelineType);
}

void PostEffect::DrawGaussianVertical(uint32_t sourceSrvIndex)
{
	DrawWithPipeline(sourceSrvIndex, PipelineType::GaussianFilterVertical);
}

void PostEffect::DrawWithPipeline(uint32_t sourceSrvIndex, PipelineType pipelineType)
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// RenderTextureのSRVが入ったDescriptorHeapを使用する
	// CopyImage用の描画設定をCommandListへ積む
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineStates_[static_cast<int>(pipelineType)].Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// PixelShaderのt0へRenderTextureのSRVを設定する
	srvManager_->SetGraphicsRootDescriptorTable(
		0,
		sourceSrvIndex);
	if (pipelineType == PipelineType::DepthBasedOutline)
	{
		srvManager_->SetGraphicsRootDescriptorTable(
			1,
			dxCommon_->GetDepthStencilSrvIndex());
	}
	else if (pipelineType == PipelineType::Dissolve || pipelineType == PipelineType::DissolveEdge)
	{
		srvManager_->SetGraphicsRootDescriptorTable(
			1,
			dissolveMask_ == DissolveMask::Noise0 ? dissolveMaskSrvIndex_ : dissolveMaskNoise1SrvIndex_);
	}
	commandList->SetGraphicsRootConstantBufferView(
		2,
		depthBasedOutlineResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		3,
		dissolveResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		4,
		randomNoiseResource_->GetGPUVirtualAddress());

	// VertexShaderのSV_VertexIDで3頂点を作るため、頂点バッファは不要
	commandList->DrawInstanced(3, 1, 0, 0);
}

void PostEffect::CreateRootSignature()
{
	// PixelShaderのt0でRenderTextureを読むためのDescriptorRange
	D3D12_DESCRIPTOR_RANGE descriptorRanges[2]{};
	descriptorRanges[0].BaseShaderRegister = 0;
	descriptorRanges[0].NumDescriptors = 1;
	descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	descriptorRanges[1].BaseShaderRegister = 1;
	descriptorRanges[1].NumDescriptors = 1;
	descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[5]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].Descriptor.ShaderRegister = 0;
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2;

	// PixelShaderのs0でRenderTextureをサンプリングする
	D3D12_STATIC_SAMPLER_DESC staticSamplers[2]{};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	staticSamplers[1] = staticSamplers[0];
	staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	staticSamplers[1].ShaderRegister = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pStaticSamplers = staticSamplers;
	rootSignatureDesc.NumStaticSamplers = _countof(staticSamplers);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob);
	assert(SUCCEEDED(hr));

	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void PostEffect::CreateGraphicsPipeline()
{
	CreateGraphicsPipelineState(PipelineType::Fullscreen, L"resources/shaders/Fullscreen.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::Grayscale, L"resources/shaders/Grayscale.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::Sepia, L"resources/shaders/Sepia.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::Vignette, L"resources/shaders/Vignette.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::RadialBlur, L"resources/shaders/RadialBlur.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::Dissolve, L"resources/shaders/DissolveNoise.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::DissolveEdge, L"resources/shaders/Dissolve.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::RandomNoise, L"resources/shaders/RandomNoise.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::LuminanceBasedOutline, L"resources/shaders/LuminanceBasedOutline.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::DepthBasedOutline, L"resources/shaders/DepthBasedOutline.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::GaussianFilterHorizontal, L"resources/shaders/GaussianFilter.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::GaussianFilterVertical, L"resources/shaders/GaussianFilterVertical.PS.hlsl");
	CreateGraphicsPipelineState(PipelineType::Smoothing, L"resources/shaders/BoxFilter.PS.hlsl");
}

void PostEffect::CreateGraphicsPipelineState(PipelineType type, const wchar_t* pixelShaderPath)
{
	// SV_VertexIDから頂点を作るため、InputLayoutには何も設定しない
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = nullptr;
	inputLayoutDesc.NumElements = 0;

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// 全画面コピーでは深度比較も深度書き込みも行わない
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
		dxCommon_->CompileShader(L"resources/shaders/Fullscreen.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
		dxCommon_->CompileShader(pixelShaderPath, L"ps_6_0");
	assert(pixelShaderBlob);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.InputLayout = inputLayoutDesc;
	pipelineDesc.VS = {
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize()
	};
	pipelineDesc.PS = {
		pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize()
	};
	pipelineDesc.BlendState = blendDesc;
	pipelineDesc.RasterizerState = rasterizerDesc;
	pipelineDesc.DepthStencilState = depthStencilDesc;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(&graphicsPipelineStates_[static_cast<int>(type)]));
	assert(SUCCEEDED(hr));
}
