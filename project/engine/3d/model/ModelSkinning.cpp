#include "Model.h"

#include "Logger.h"
#include "SrvManager.h"
#include "TextureManager.h"

#include <algorithm>
#include <cassert>
#include <cstring>

using namespace MyMath;

namespace Engine
{

// SkinClusterのGPU Resource作成、Palette更新、Compute Skinning、Skinned描画をまとめる。
// 通常モデルの読込・描画と分け、骨ありモデルの処理だけを追えるようにする。
// 生成途中の失敗はDescriptorを返し、完成した時だけObject3dへ渡す。
// 骨付きModelの頂点・WeightとPaletteをGPUへ渡し、生成失敗時は未完成の状態を採用しません。
bool Model::CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const Skeleton& skeleton, const ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, SkinCluster& result)
{
	// 空の頂点や未初期化Deviceでは、Descriptorを取得する前に生成を止めます。
	if (!device || !descriptorHeap || descriptorSize == 0 || !modelCommon_ ||
		!modelCommon_->GetDxCommon() || !vertexResource_ ||
		skeleton.joints.empty() || modelData.vertices.empty()) {
		Logger::Log("Model: SkinCluster dependencies are unavailable.");
		return false;
	}
	SkinCluster skinCluster{};
	SrvManager* srvManager = SrvManager::GetInstance();
	// 途中失敗時は、Map先・Buffer・取得済みDescriptorを同じ所有者から片付けます。
	auto fail = [&](const char* message) {
		Logger::Log(message);
		if (skinCluster.paletteResource && skinCluster.mappedPalette.data()) {
			skinCluster.paletteResource->Unmap(0, nullptr);
		}
		if (skinCluster.influenceResource && skinCluster.mappedInfluence.data()) {
			skinCluster.influenceResource->Unmap(0, nullptr);
		}
		if (skinCluster.skinningInformationResource && skinCluster.mappedSkinningInformation) {
			skinCluster.skinningInformationResource->Unmap(0, nullptr);
		}
		for (uint32_t index : { skinCluster.paletteSrvIndex, skinCluster.inputVertexSrvIndex,
			skinCluster.influenceSrvIndex, skinCluster.outputVertexUavIndex }) {
			if (index != UINT32_MAX) {
				srvManager->Free(index);
			}
		}
		return false;
	};
	//マネージャから次使っていい番号をもらう
	uint32_t srvIndex = srvManager->Allocate();
	if (srvIndex == SrvManager::kInvalidSrvIndex) {
		return fail("Model: no palette SRV available for SkinCluster.");
	}
	skinCluster.paletteSrvIndex = srvIndex;

	// palette用のResourceを確保
	skinCluster.paletteResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(WellForGPU) * skeleton.joints.size());
	if (!skinCluster.paletteResource) {
		return fail("Model: SkinCluster palette buffer creation failed.");
	}
	WellForGPU* mappedPalette = nullptr;
	const HRESULT paletteMapResult = skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	if (FAILED(paletteMapResult) || !mappedPalette) {
		return fail("Model: SkinCluster palette buffer Map failed.");
	}
	skinCluster.mappedPalette = { mappedPalette,skeleton.joints.size() };//spanを使ってアクセスするようにする
	skinCluster.paletteSrvHandle.first = srvManager->GetCPUDescriptorHandle(srvIndex);
	skinCluster.paletteSrvHandle.second = srvManager->GetGPUDescriptorHandle(srvIndex);

	//palette用のsrvを作成する。StructuredBufferでアクセスできるようにする。
	D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
	paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	paletteSrvDesc.Buffer.FirstElement = 0;
	paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	paletteSrvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
	paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
	device->CreateShaderResourceView(skinCluster.paletteResource.Get(), &paletteSrvDesc, skinCluster.paletteSrvHandle.first);

	uint32_t inputVertexSrvIndex = srvManager->Allocate();
	if (inputVertexSrvIndex == SrvManager::kInvalidSrvIndex) {
		return fail("Model: no input vertex SRV available for SkinCluster.");
	}
	skinCluster.inputVertexSrvIndex = inputVertexSrvIndex;
	skinCluster.inputVertexSrvHandle.first = srvManager->GetCPUDescriptorHandle(inputVertexSrvIndex);
	skinCluster.inputVertexSrvHandle.second = srvManager->GetGPUDescriptorHandle(inputVertexSrvIndex);
	srvManager->CreateSRVforStructuredBuffer(
		inputVertexSrvIndex, vertexResource_.Get(), UINT(modelData.vertices.size()), sizeof(VertexData));

	//influence用のResourceを確保。頂点ごとにinfluence情報を追加できるようにする
	skinCluster.influenceResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexInfluence) * modelData.vertices.size());
	if (!skinCluster.influenceResource) {
		return fail("Model: SkinCluster influence buffer creation failed.");
	}
	VertexInfluence* mappedInfluence = nullptr;
	const HRESULT influenceMapResult = skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	if (FAILED(influenceMapResult) || !mappedInfluence) {
		return fail("Model: SkinCluster influence buffer Map failed.");
	}
	std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * modelData.vertices.size());//0埋め weightを0にしておく
	skinCluster.mappedInfluence = { mappedInfluence,modelData.vertices.size() };

	//Influence用のVBVを作成
	skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
	skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * modelData.vertices.size());
	skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

	uint32_t influenceSrvIndex = srvManager->Allocate();
	if (influenceSrvIndex == SrvManager::kInvalidSrvIndex) {
		return fail("Model: no influence SRV available for SkinCluster.");
	}
	skinCluster.influenceSrvIndex = influenceSrvIndex;
	skinCluster.influenceSrvHandle.first = srvManager->GetCPUDescriptorHandle(influenceSrvIndex);
	skinCluster.influenceSrvHandle.second = srvManager->GetGPUDescriptorHandle(influenceSrvIndex);
	srvManager->CreateSRVforStructuredBuffer(
		influenceSrvIndex, skinCluster.influenceResource.Get(), UINT(modelData.vertices.size()), sizeof(VertexInfluence));

	D3D12_HEAP_PROPERTIES outputVertexHeapProperties{};
	outputVertexHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC outputVertexResourceDesc{};
	outputVertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	outputVertexResourceDesc.Width = sizeof(VertexData) * modelData.vertices.size();
	outputVertexResourceDesc.Height = 1;
	outputVertexResourceDesc.DepthOrArraySize = 1;
	outputVertexResourceDesc.MipLevels = 1;
	outputVertexResourceDesc.SampleDesc.Count = 1;
	outputVertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	outputVertexResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	HRESULT hr = device->CreateCommittedResource(
		&outputVertexHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&outputVertexResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&skinCluster.outputVertexResource));
	if (FAILED(hr) || !skinCluster.outputVertexResource) {
		return fail("Model: SkinCluster output vertex buffer creation failed.");
	}

	skinCluster.outputVertexBufferView.BufferLocation = skinCluster.outputVertexResource->GetGPUVirtualAddress();
	skinCluster.outputVertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	skinCluster.outputVertexBufferView.StrideInBytes = sizeof(VertexData);

	uint32_t outputVertexUavIndex = srvManager->Allocate();
	if (outputVertexUavIndex == SrvManager::kInvalidSrvIndex) {
		return fail("Model: no output vertex UAV available for SkinCluster.");
	}
	skinCluster.outputVertexUavIndex = outputVertexUavIndex;
	skinCluster.outputVertexUavHandle.first = srvManager->GetCPUDescriptorHandle(outputVertexUavIndex);
	skinCluster.outputVertexUavHandle.second = srvManager->GetGPUDescriptorHandle(outputVertexUavIndex);

	D3D12_UNORDERED_ACCESS_VIEW_DESC outputVertexUavDesc{};
	outputVertexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
	outputVertexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	outputVertexUavDesc.Buffer.FirstElement = 0;
	outputVertexUavDesc.Buffer.NumElements = UINT(modelData.vertices.size());
	outputVertexUavDesc.Buffer.CounterOffsetInBytes = 0;
	outputVertexUavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	outputVertexUavDesc.Buffer.StructureByteStride = sizeof(VertexData);
	device->CreateUnorderedAccessView(
		skinCluster.outputVertexResource.Get(), nullptr, &outputVertexUavDesc, skinCluster.outputVertexUavHandle.first);

	skinCluster.skinningInformationResource =
		modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(SkinCluster::SkinningInformation));
	if (!skinCluster.skinningInformationResource) {
		return fail("Model: SkinCluster information buffer creation failed.");
	}
	const HRESULT informationMapResult = skinCluster.skinningInformationResource->Map(
		0, nullptr, reinterpret_cast<void**>(&skinCluster.mappedSkinningInformation));
	if (FAILED(informationMapResult) || !skinCluster.mappedSkinningInformation) {
		return fail("Model: SkinCluster information buffer Map failed.");
	}
	skinCluster.mappedSkinningInformation->numVertices = UINT(modelData.vertices.size());

	//InverseBindPoseMatrixを格納する場所を作成して、単位行列で埋める
	skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
	std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(), MakeIdentity4x4);

	//ModelDataのSkinCluster情報を解析してInfluenceの中身を埋める
	for (const auto& jointWeight : modelData.skinClusterData)//ModelのSkinClusterの情報を解析
	{
		auto it = skeleton.jointMap.find(jointWeight.first);//jointWeight.firstはjoint名なので、skeletonに対象となるjointが含まれているか判断
		if (it == skeleton.jointMap.end())//そんな名前のJointは存在しない。なので次に回す
		{
			continue;
		}
		if (it->second < 0 || static_cast<size_t>(it->second) >= skinCluster.inverseBindPoseMatrices.size()) {
			return fail("Model: SkinCluster joint index is out of range.");
		}
		//(*it).secondにはjointのindexが入ってるので、該当のindexのinverseBindPoseMatrixを代入
		skinCluster.inverseBindPoseMatrices[(*it).second] = jointWeight.second.inverseBindPoseMatrix;
		for (const auto& vertexWeight : jointWeight.second.vertexWeights)
		{
			if (vertexWeight.vertexIndex >= skinCluster.mappedInfluence.size()) {
				return fail("Model: SkinCluster vertex index is out of range.");
			}
			auto& currentInfluence = skinCluster.mappedInfluence[vertexWeight.vertexIndex];//該当のvertexIndexのinfluence情報を参照しておく
			for (uint32_t index = 0; index < kNumMaxInfluence; ++index)//空いてるところに入れる
			{
				if (currentInfluence.weights[index] == 0.0f)//weight==0が空いてる状態なので、その場にweightとjointのindexを代入
				{
					currentInfluence.weights[index] = vertexWeight.weight;
					currentInfluence.jointIndices[index] = (*it).second;
					break;//空きに入れて処理を終えたら、次のindexを探すループは抜ける
				}
			}
		}
	}

	result = std::move(skinCluster);
	return true;
}

void Model::Update(SkinCluster& skinCluster, const Skeleton& skeleton)
{
	if (skinCluster.mappedPalette.data() == nullptr) {
		return;
	}

	const size_t jointCount = (std::min)(skeleton.joints.size(), skinCluster.inverseBindPoseMatrices.size());
	for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex)
	{
		skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix =
			Multiply(skinCluster.inverseBindPoseMatrices[jointIndex], skeleton.joints[jointIndex].skeletonSpaceMatrix);
		skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix =
			Transpose(Inverse(skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix));
	}
}

// Graphics Skinningの頂点とWeightを二つの入力へ渡し、材質ごとにMeshを描きます。
void Model::Draw(const SkinCluster& skinCluster, uint32_t textureSrvIndexOverride)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	// 1. 頂点バッファビューを設定 
	D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
		vertexBufferView_,                // スロット0: 位置・UV・法線
		skinCluster.influenceBufferView  // スロット1: Weight・Index
	};
	commandList->IASetVertexBuffers(0, 2, vbvs);

	// インデックスバッファをセット
	commandList->IASetIndexBuffer(&indexBufferView_);

	// 2. 形状を設定 (三角形リスト)
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 3. マテリアルCBufferの設定 (RootParameter 0)
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// マトリックスパレットSRVの設定 (RootParameter 7)
	// （※Object3dCommonで追加した [7] 番のパラメータに、パレットのSRVを渡します）
	commandList->SetGraphicsRootDescriptorTable(7, skinCluster.paletteSrvHandle.second);

	for (const MeshData& mesh : modelData_.meshes)
	{
		const MaterialData& material = modelData_.materials[mesh.materialIndex];
		D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = textureSrvIndexOverride != UINT32_MAX
			? SrvManager::GetInstance()->GetGPUDescriptorHandle(textureSrvIndexOverride)
			: TextureManager::GetInstance()->GetSrvHandleGPU(material.textureFilePath);
		commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);
		commandList->DrawIndexedInstanced(mesh.indexCount, 1, mesh.indexOffset, 0, 0);
	}

}

void Model::DispatchSkinning(SkinCluster& skinCluster)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	if (skinCluster.outputVertexResourceState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = skinCluster.outputVertexResource.Get();
		barrier.Transition.StateBefore = skinCluster.outputVertexResourceState;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
		skinCluster.outputVertexResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	commandList->SetComputeRootDescriptorTable(0, skinCluster.paletteSrvHandle.second);
	commandList->SetComputeRootDescriptorTable(1, skinCluster.inputVertexSrvHandle.second);
	commandList->SetComputeRootDescriptorTable(2, skinCluster.influenceSrvHandle.second);
	commandList->SetComputeRootDescriptorTable(3, skinCluster.outputVertexUavHandle.second);
	commandList->SetComputeRootConstantBufferView(4, skinCluster.skinningInformationResource->GetGPUVirtualAddress());

	const uint32_t threadGroupCount = (skinCluster.mappedSkinningInformation->numVertices + 1023) / 1024;
	commandList->Dispatch(threadGroupCount, 1, 1);

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = skinCluster.outputVertexResource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
	skinCluster.outputVertexResourceState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
}

// Compute Skinning済みの頂点Bufferを読み、通常の材質と同じMesh順で描画します。
void Model::DrawSkinned(const SkinCluster& skinCluster, uint32_t textureSrvIndexOverride)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	commandList->IASetVertexBuffers(0, 1, &skinCluster.outputVertexBufferView);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	for (const MeshData& mesh : modelData_.meshes)
	{
		const MaterialData& material = modelData_.materials[mesh.materialIndex];
		D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = textureSrvIndexOverride != UINT32_MAX
			? SrvManager::GetInstance()->GetGPUDescriptorHandle(textureSrvIndexOverride)
			: TextureManager::GetInstance()->GetSrvHandleGPU(material.textureFilePath);
		commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);
		commandList->DrawIndexedInstanced(mesh.indexCount, 1, mesh.indexOffset, 0, 0);
	}
}

}

