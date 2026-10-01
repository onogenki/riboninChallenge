#include "Object3dCommon.h"
#include <cassert>
#include <vector>

void Object3dCommon::Initialize(DirectXCommon* dxCommon)
{
	assert(dxCommon);
	dxCommon_ = dxCommon;

	// ルートシグネチャの生成
	CreateRootSignature();
	// グラフィックスパイプラインの生成
	CreateGraphicsPipeline();
	CreateOccludedSilhouetteGraphicsPipeline();
	CreateMirrorRootSignature();
	CreateMirrorGraphicsPipeline();

	// 骨ありModelは、元から使っていたVertex Shader方式で頂点を変形します。
	// Compute Shader方式は比較用として残しますが、通常描画には使いません。
	CreateSkinningRootSignature();
	CreateSkinningGraphicsPipeline();

	// Compute Shader方式の設定も、将来の比較・検証用に初期化します。
	CreateSkinningComputeRootSignature();
	CreateSkinningComputePipeline();

	CreateSkyboxRootSignature();
	CreateSkyboxPipeline();
}

void Object3dCommon::SetCommonDrawSetting()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	//ルートシグネチャをセットするコマンド
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	//グラフィックスパイプラインステートをセットするコマンド
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	//プリミティブトポロジーをセットするコマンド
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// 通常の壁より奥にある部分だけを描く、壁越しシルエット用のPipelineを設定します。
void Object3dCommon::SetOccludedSilhouetteDrawSetting()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(occludedSilhouetteGraphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::SetSkinningCommonDrawSetting()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	//スキニング用のルートシグネチャとパイプラインステートをセット
	commandList->SetGraphicsRootSignature(skinningRootSignature_.Get());
	commandList->SetPipelineState(skinningGraphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::SetSkinningComputeSetting()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	commandList->SetComputeRootSignature(skinningComputeRootSignature_.Get());
	commandList->SetPipelineState(skinningComputePipelineState_.Get());
}

void Object3dCommon::SetMirrorDrawSetting()
{
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(mirrorRootSignature_.Get());
	commandList->SetPipelineState(mirrorGraphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::CreateRootSignature()
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
	rootParameters[4].Descriptor.RegisterSpace = 0;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う

	//[5]PointLight
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[5].Descriptor.ShaderRegister = 3;                    // b3 に対応
	rootParameters[5].Descriptor.RegisterSpace = 0;

	//[6]SpotLight
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[6].Descriptor.ShaderRegister = 4;                    // b4 に対応
	rootParameters[6].Descriptor.RegisterSpace = 0;

	//[7] EnvironmentMap(TextureCube t1)
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[7].DescriptorTable.pDescriptorRanges = environmentDescriptorRange;
	rootParameters[7].DescriptorTable.NumDescriptorRanges = _countof(environmentDescriptorRange);

	// [8] Occluded Silhouette Color (CBV)
	rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[8].Descriptor.ShaderRegister = 5;

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
		IID_PPV_ARGS(&rootSignature_)); // メンバ変数 rootSignature_ に保存
	assert(SUCCEEDED(hr));
}
// グラフィックスパイプラインの生成
void Object3dCommon::CreateGraphicsPipeline()
{
	HRESULT hr = S_FALSE;

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	// 入力レイアウトの追加
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

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
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"resources/shaders/Object3d.VS.hlsl",
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
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get(); // RootSignature
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
		IID_PPV_ARGS(&graphicsPipelineState_)); // メンバ変数 graphicsPipelineState_ に保存
	assert(SUCCEEDED(hr));
}

// 通常Modelと同じ頂点形式で、壁に隠れた部分だけを半透明色で描くPipelineを作成します。
void Object3dCommon::CreateOccludedSilhouetteGraphicsPipeline()
{
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

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/Object3d.VS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(
		L"resources/shaders/OccludedSilhouette.PS.hlsl", L"ps_6_0");
	assert(vertexShaderBlob != nullptr);
	assert(pixelShaderBlob != nullptr);

	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	// 既に描かれた壁の深度を保持し、シルエット自身は深度を書き換えません。
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	// 壁より奥にあるPixelだけを通すため、通常のLESSではなくGREATERを使います。
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
	pipelineDesc.pRootSignature = rootSignature_.Get();
	pipelineDesc.InputLayout = inputLayoutDesc;
	pipelineDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	pipelineDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	pipelineDesc.BlendState = blendDesc;
	pipelineDesc.RasterizerState = rasterizerDesc;
	pipelineDesc.DepthStencilState = depthStencilDesc;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	const HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
		&pipelineDesc,
		IID_PPV_ARGS(&occludedSilhouetteGraphicsPipelineState_));
	assert(SUCCEEDED(hr));
}
