#include "Object3dCommon.h"

#include <cassert>
#include <vector>


// 平面鏡へ反射Textureを描くためのRootSignatureとGraphics Pipelineをまとめる。
void Object3dCommon::CreateMirrorRootSignature()
{
	D3D12_DESCRIPTOR_RANGE textureRange{};
	textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	textureRange.NumDescriptors = 1;
	textureRange.BaseShaderRegister = 0;
	textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3]{};
	// VertexShaderのb0には、通常Objectと同じ座標変換行列を渡します。
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	// PixelShaderのb0には、反射CameraのViewProjectionを渡します。
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].Descriptor.ShaderRegister = 0;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &textureRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.pParameters = rootParameters;
	desc.NumParameters = _countof(rootParameters);
	desc.pStaticSamplers = &sampler;
	desc.NumStaticSamplers = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(
		&desc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob);
	assert(SUCCEEDED(hr));
	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&mirrorRootSignature_));
	assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateMirrorGraphicsPipeline()
{
	D3D12_INPUT_ELEMENT_DESC inputElements[3]{};
	inputElements[0].SemanticName = "POSITION";
	inputElements[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElements[1].SemanticName = "TEXCOORD";
	inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElements[2].SemanticName = "NORMAL";
	inputElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	const D3D12_INPUT_LAYOUT_DESC inputLayout{
		inputElements,
		_countof(inputElements),
	};
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShader = dxCommon_->CompileShader(
		L"resources/shaders/Mirror.VS.hlsl",
		L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShader = dxCommon_->CompileShader(
		L"resources/shaders/Mirror.PS.hlsl",
		L"ps_6_0");
	assert(vertexShader && pixelShader);

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.DepthClipEnable = true;
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.pRootSignature = mirrorRootSignature_.Get();
	pipelineDesc.InputLayout = inputLayout;
	pipelineDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	pipelineDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
	pipelineDesc.BlendState = blendDesc;
	pipelineDesc.RasterizerState = rasterizerDesc;
	pipelineDesc.DepthStencilState = depthStencilDesc;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	const HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(&mirrorGraphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

