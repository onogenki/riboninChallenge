#include "Object3dCommon.h"

#include <cassert>
#include <vector>


// 骨ありモデルのGraphics SkinningとCompute SkinningのPipelineをまとめる。
void Object3dCommon::CreateSkinningRootSignature()
{
	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// DescriptorRangeの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1;     // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	D3D12_DESCRIPTOR_RANGE environmentDescriptorRange[1] = {};
	environmentDescriptorRange[0].BaseShaderRegister = 1;
	environmentDescriptorRange[0].NumDescriptors = 1;
	environmentDescriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	environmentDescriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE paletteRange[1] = {};
	paletteRange[0].BaseShaderRegister = 0;//t0
	paletteRange[0].NumDescriptors = 1;
	paletteRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	paletteRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameter作成。複数設定できるので配列
	D3D12_ROOT_PARAMETER rootParameters[9] = {};

	// [0] Material (CBV)
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号0とバインド

	// [1] TransformationMatrix (CBV)
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VertexShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号0を使う

	// [2] Texture (DescriptorTable)
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数

	// [3] DirectionalLight (CBV)
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号1を使う

	//[4]Camera
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;    // 定数バッファ
	rootParameters[4].Descriptor.ShaderRegister = 2;                    // b2 に対応
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う

	//[5]PointLight
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[5].Descriptor.ShaderRegister = 3;                    // b3 に対応

	//[6]SpotLight
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[6].Descriptor.ShaderRegister = 4;                    // b4 に対応

	//[7] マトリックスパレット (VertexShader用 SRV テーブル)
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // VSで使う
	rootParameters[7].DescriptorTable.pDescriptorRanges = paletteRange;
	rootParameters[7].DescriptorTable.NumDescriptorRanges = _countof(paletteRange);

	//[8] EnvironmentMap(TextureCube t1)
	rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[8].DescriptorTable.pDescriptorRanges = environmentDescriptorRange;
	rootParameters[8].DescriptorTable.NumDescriptorRanges = _countof(environmentDescriptorRange);

	descriptionRootSignature.pParameters = rootParameters; // ルートパレメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters); // 配列の長さ

	// StaticSamplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0～1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // Mipmapを使う
	staticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		assert(false);
	}

	// バイナリを元に生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&skinningRootSignature_)); // メンバ変数 rootSignature_ に保存
	assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateSkinningGraphicsPipeline()
{
	HRESULT hr = S_FALSE;

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[5] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[0].InputSlot = 0;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].InputSlot = 0;
	// 入力レイアウトの追加
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].InputSlot = 0;

	inputElementDescs[3].SemanticName = "WEIGHT";
	inputElementDescs[3].SemanticIndex = 0;
	inputElementDescs[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;//float32_t4
	inputElementDescs[3].InputSlot = 1;//1番目のslotのVBVのことだと伝える
	inputElementDescs[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[4].SemanticName = "INDEX";
	inputElementDescs[4].SemanticIndex = 0;
	inputElementDescs[4].Format = DXGI_FORMAT_R32G32B32A32_SINT;//int32_t4
	inputElementDescs[4].InputSlot = 1;//1番目のslotのVBVのことだと伝える
	inputElementDescs[4].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasiterZerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// カリングしない(裏面表示しない)
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/SkinningObject3d.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3d.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// PSOを作成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = skinningRootSignature_.Get(); // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() }; // VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() }; // PixelShader

	graphicsPipelineStateDesc.BlendState = blendDesc; // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState

	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ(形状)のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むのかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// 実際に生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(&skinningGraphicsPipelineState_)); // メンバ変数 graphicsPipelineState_ に保存
	assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateSkinningComputeRootSignature()
{
	D3D12_DESCRIPTOR_RANGE descriptorRanges[4] = {};
	for (uint32_t index = 0; index < 3; ++index)
	{
		descriptorRanges[index].BaseShaderRegister = index;
		descriptorRanges[index].NumDescriptors = 1;
		descriptorRanges[index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRanges[index].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}
	descriptorRanges[3].BaseShaderRegister = 0;
	descriptorRanges[3].NumDescriptors = 1;
	descriptorRanges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	descriptorRanges[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[5] = {};
	for (uint32_t index = 0; index < 4; ++index)
	{
		rootParameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[index].DescriptorTable.pDescriptorRanges = &descriptorRanges[index];
		rootParameters[index].DescriptorTable.NumDescriptorRanges = 1;
	}
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[4].Descriptor.ShaderRegister = 0;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));

	hr = dxCommon_->GetDevice()->CreateRootSignature(0,
		signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&skinningComputeRootSignature_));
	assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateSkinningComputePipeline()
{
	Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/Skinning.CS.hlsl", L"cs_6_0");
	assert(computeShaderBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
	computePipelineStateDesc.CS = {
		computeShaderBlob->GetBufferPointer(),
		computeShaderBlob->GetBufferSize()
	};
	computePipelineStateDesc.pRootSignature = skinningComputeRootSignature_.Get();

	HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(
		&computePipelineStateDesc, IID_PPV_ARGS(&skinningComputePipelineState_));
	assert(SUCCEEDED(hr));
}

