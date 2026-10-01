#include "Model.h"

#include "SrvManager.h"
#include "TextureManager.h"

#include <algorithm>
#include <cassert>
#include <cstring>

using namespace MyMath;

// SkinClusterのGPU Resource作成、Palette更新、Compute Skinning、Skinned描画をまとめる。
// 通常モデルの読込・描画と分け、骨ありモデルの処理だけを追えるようにする。
Model::SkinCluster Model::CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const Skeleton& skeleton, const ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize)
{
	SkinCluster skinCluster;
	//マネージャから次使っていい番号をもらう
	uint32_t srvIndex = SrvManager::GetInstance()->Allocate();
	skinCluster.paletteSrvIndex = srvIndex;

	//paletter用のResourceを確保
	skinCluster.paletteResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(WellForGPU) * skeleton.joints.size());
	WellForGPU* mappedPalette = nullptr;
	skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	skinCluster.mappedPalette = { mappedPalette,skeleton.joints.size() };//spanを使ってアクセスするようにする
	skinCluster.paletteSrvHandle.first = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex);
	skinCluster.paletteSrvHandle.second = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);

	//palette用のsrvを作成する。StructuredBufferでアクセスできるようにする。
	D3D12_SHADER_RESOURCE_VIEW_DESC paletterSrvDesc{};
	paletterSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	paletterSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	paletterSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	paletterSrvDesc.Buffer.FirstElement = 0;
	paletterSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	paletterSrvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
	paletterSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
	device->CreateShaderResourceView(skinCluster.paletteResource.Get(), &paletterSrvDesc, skinCluster.paletteSrvHandle.first);

	uint32_t inputVertexSrvIndex = SrvManager::GetInstance()->Allocate();
	skinCluster.inputVertexSrvIndex = inputVertexSrvIndex;
	skinCluster.inputVertexSrvHandle.first = SrvManager::GetInstance()->GetCPUDescriptorHandle(inputVertexSrvIndex);
	skinCluster.inputVertexSrvHandle.second = SrvManager::GetInstance()->GetGPUDescriptorHandle(inputVertexSrvIndex);
	SrvManager::GetInstance()->CreateSRVforStructuredBuffer(
		inputVertexSrvIndex, vertexResource.Get(), UINT(modelData.vertices.size()), sizeof(VertexData));

	//influence用のResourceを確保。頂点ごとにinfluence情報を追加できるようにする
	skinCluster.influenceResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexInfluence) * modelData.vertices.size());
	VertexInfluence* mappedInfluence = nullptr;
	skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * modelData.vertices.size());//0埋め weightを0にしておく
	skinCluster.mappedInfluence = { mappedInfluence,modelData.vertices.size() };

	//Influence用のVBVを作成
	skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
	skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * modelData.vertices.size());
	skinCluster.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);

	uint32_t influenceSrvIndex = SrvManager::GetInstance()->Allocate();
	skinCluster.influenceSrvIndex = influenceSrvIndex;
	skinCluster.influenceSrvHandle.first = SrvManager::GetInstance()->GetCPUDescriptorHandle(influenceSrvIndex);
	skinCluster.influenceSrvHandle.second = SrvManager::GetInstance()->GetGPUDescriptorHandle(influenceSrvIndex);
	SrvManager::GetInstance()->CreateSRVforStructuredBuffer(
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
	assert(SUCCEEDED(hr));

	skinCluster.outputVertexBufferView.BufferLocation = skinCluster.outputVertexResource->GetGPUVirtualAddress();
	skinCluster.outputVertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());
	skinCluster.outputVertexBufferView.StrideInBytes = sizeof(VertexData);

	uint32_t outputVertexUavIndex = SrvManager::GetInstance()->Allocate();
	skinCluster.outputVertexUavIndex = outputVertexUavIndex;
	skinCluster.outputVertexUavHandle.first = SrvManager::GetInstance()->GetCPUDescriptorHandle(outputVertexUavIndex);
	skinCluster.outputVertexUavHandle.second = SrvManager::GetInstance()->GetGPUDescriptorHandle(outputVertexUavIndex);

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
	skinCluster.skinningInformationResource->Map(
		0, nullptr, reinterpret_cast<void**>(&skinCluster.mappedSkinningInformation));
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
		//(*it).secondにはjointのindexが入ってるので、該当のindexのinverseBindPoseMatrixを代入
		skinCluster.inverseBindPoseMatrices[(*it).second] = jointWeight.second.inverseBindPoseMatrix;
		for (const auto& vertexWeight : jointWeight.second.vertexWeights)
		{
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

	return skinCluster;
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

void Model::Draw(const SkinCluster& skinCluster, uint32_t textureSrvIndexOverride)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	// 1. 頂点バッファビューを設定 
	D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
		vertexBufferView,                // スロット0: 位置・UV・法線
		skinCluster.influenceBufferView  // スロット1: Weight・Index
	};
	commandList->IASetVertexBuffers(0, 2, vbvs);

	// インデックスバッファをセット
	commandList->IASetIndexBuffer(&indexBufferView);

	// 2. 形状を設定 (三角形リスト)
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 3. マテリアルCBufferの設定 (RootParameter 0)
	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

	// マトリックスパレットSRVの設定 (RootParameter 7)
	// （※Object3dCommonで追加した [7] 番のパラメータに、パレットのSRVを渡します）
	commandList->SetGraphicsRootDescriptorTable(7, skinCluster.paletteSrvHandle.second);

	for (const MeshData& mesh : modelData.meshes)
	{
		const MaterialData& material = modelData.materials[mesh.materialIndex];
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

void Model::DrawSkinned(const SkinCluster& skinCluster, uint32_t textureSrvIndexOverride)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	commandList->IASetVertexBuffers(0, 1, &skinCluster.outputVertexBufferView);
	commandList->IASetIndexBuffer(&indexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());

	for (const MeshData& mesh : modelData.meshes)
	{
		const MaterialData& material = modelData.materials[mesh.materialIndex];
		D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = textureSrvIndexOverride != UINT32_MAX
			? SrvManager::GetInstance()->GetGPUDescriptorHandle(textureSrvIndexOverride)
			: TextureManager::GetInstance()->GetSrvHandleGPU(material.textureFilePath);
		commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);
		commandList->DrawIndexedInstanced(mesh.indexCount, 1, mesh.indexOffset, 0, 0);
	}
}

