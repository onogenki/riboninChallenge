#include "Model.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cassert>
#include <filesystem>

using namespace MyMath;

Model::Node Model::ReadNode(aiNode* node)
{
	Node result;
	aiVector3D scale, translate;
	aiQuaternion rotate;
	node->mTransformation.Decompose(scale, rotate, translate);//assimpの行列からSRTを抽出する関数を利用
	result.transform.scale = { scale.x,scale.y,scale.z };//scaleはそのまま
	result.transform.rotate = { rotate.x,-rotate.y,-rotate.z,rotate.w };//X軸を反転、さらに回転方向が逆なので軸を反転させる
	result.transform.translate = { -translate.x,translate.y,translate.z };//X軸を反転
	result.localMatrix = MakeAffineMatrixQuaternion(result.transform.scale, result.transform.rotate, result.transform.translate);

	result.name = node->mName.C_Str();//Node名を格納
	result.children.resize(node->mNumChildren);//子供の数だけ確保
	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex)
	{
		//再帰的に読んで階層構造体を作っていく
		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}

	return result;
}

bool Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename)
{

	//ModelCommonのポインタを引数からメンバ変数に記録する
	modelCommon_ = modelCommon;
	//モデル読み込み
	if (!LoadModelFile(directoryPath, filename)) {
		return false;
	}

	CreateIndexData();
	//頂点データ作成
	CreateVertexData();
	//マテリアルデータ作成
	CreateMaterialData();
	for (MaterialData& material : modelData.materials)
	{
		const std::filesystem::path texturePath = material.textureFilePath;
		if (material.textureFilePath.empty() ||
			material.textureFilePath.find("None") != std::string::npos ||
			material.textureFilePath.front() == '*' ||
			!std::filesystem::exists(texturePath))
		{
			material.textureFilePath = "Resources/debug/uvChecker.png";
		}
		TextureManager::GetInstance()->LoadTexture(material.textureFilePath);
	}
	return true;
}

void Model::Draw(uint32_t textureSrvIndexOverride)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	// 1. 頂点バッファビューを設定 (通常のVBVは1つだけ)
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

	// インデックスバッファをセット
	commandList->IASetIndexBuffer(&indexBufferView);

	// 2. 形状を設定 (三角形リスト)
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 3. マテリアルCBufferの設定 (RootParameter 0)
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

void Model::DrawGeometry()
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->IASetIndexBuffer(&indexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (const MeshData& mesh : modelData.meshes) {
		commandList->DrawIndexedInstanced(mesh.indexCount, 1, mesh.indexOffset, 0, 0);
	}
}

void Model::SetTexture(const std::string& filePath)
{//新しいテクスチャを読み込んで
	TextureManager::GetInstance()->LoadTexture(filePath);
	for (MaterialData& material : modelData.materials)
	{
		material.textureFilePath = filePath;
	}
}

//アニメーション適用
void Model::CreateIndexData()
{
	assert(!modelData.indices.empty());
	// --- インデックスバッファの作成 ---
	indexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * modelData.indices.size());

	// インデックスバッファビューの設定
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	indexBufferView.SizeInBytes = sizeof(uint32_t) * static_cast<uint32_t>(modelData.indices.size());
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	// --- データのマップとコピー ---
	uint32_t* mappedIndex = nullptr;
	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	std::memcpy(mappedIndex, modelData.indices.data(), sizeof(uint32_t) * modelData.indices.size());
	indexResource->Unmap(0, nullptr);
}

void Model::CreateVertexData()
{
	// 書き込むデータのサイズ（頂点データ全体のバイト数）
	size_t sizeInBytes = sizeof(VertexData) * modelData.vertices.size();

	// 1. 頂点リソースを作る (Spriteから持ってきたヘルパー関数を使用)
	vertexResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeInBytes);

	// 2. 頂点バッファビューを作成
	// リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソース of サイズ
	vertexBufferView.SizeInBytes = static_cast<UINT>(sizeInBytes);
	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 3. データを書き込む
	// 書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	// 読み込んだモデルデータ(std::vector)の中身を、マップしたメモリ(vertexData)へ一括コピー
	std::memcpy(vertexData, modelData.vertices.data(), sizeInBytes);
}

void Model::CreateMaterialData()
{
	// 1. マテリアルリソースを作る (サイズは Material 構造体1つ分)
	materialResource = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));

	// 2. データを書き込むためのアドレスを取得して materialData に割り当てる
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	// 3. マテリアルデータの初期値を書き込む
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	materialData->enableLighting = true;

	materialData->shininess = 50.0f;

	materialData->environmentCoefficient = 0.0f;
	// 通常のModelはLightの反射をそのまま使います。
	materialData->specularIntensity = 1.0f;

	materialData->uvTransform = MakeIdentity4x4();
}

bool Model::LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
	// メンバ変数をクリアしておく
	modelData.vertices.clear();
	modelData.indices.clear();
	modelData.materials.clear();
	modelData.meshes.clear();

	//assimpでobjを読む
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;

	const aiScene* scene = importer.ReadFile(
		filePath.c_str(),
		aiProcess_FlipWindingOrder |
		aiProcess_FlipUVs |
		aiProcess_Triangulate |
		aiProcess_GenNormals);
	if (!scene || !scene->HasMeshes()) {
		return false;
	}
	if (!scene->mRootNode) {
		return false;
	}

	modelData.rootNode = ReadNode(scene->mRootNode);
	const std::filesystem::path modelPath = std::filesystem::path(directoryPath) / filename;
	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
	{
		MaterialData materialData{};
		aiMaterial* material = scene->mMaterials[materialIndex];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0)
		{
			aiString textureFilePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			const std::filesystem::path texturePath = std::filesystem::path(textureFilePath.C_Str());
			materialData.textureFilePath = texturePath.is_absolute()
				? texturePath.generic_string()
				: (modelPath.parent_path() / texturePath).generic_string();
		}
		modelData.materials.push_back(std::move(materialData));
	}
	if (modelData.materials.empty())
	{
		modelData.materials.push_back({});
	}

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)//mesh解析
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];
		//現在の頂点数を保存しておく(複数メッシュ対応のため)
		if (!mesh->HasPositions() || !mesh->HasFaces()) {
			continue;
		}
		uint32_t vertexOffset = static_cast<uint32_t>(modelData.vertices.size());
		MeshData meshData{};
		meshData.indexOffset = static_cast<uint32_t>(modelData.indices.size());
		meshData.materialIndex = (std::min)(mesh->mMaterialIndex, static_cast<uint32_t>(modelData.materials.size() - 1));

		//まず頂点をすべて解析して配列に突っ込む
		for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
		{
			aiVector3D& position = mesh->mVertices[v];
			const aiVector3D normal = mesh->HasNormals() ? mesh->mNormals[v] : aiVector3D(0.0f, 1.0f, 0.0f);
			const aiVector3D texcoord = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][v] : aiVector3D(0.0f, 0.0f, 0.0f);

			VertexData vertex;
			vertex.position = { position.x,position.y,position.z,1.0f };
			vertex.normal = { normal.x,normal.y,normal.z };
			vertex.texcoord = { texcoord.x,texcoord.y };
			//aiProcess_MakeLeftHandedはz*=-1で右手->左手に変換するので手動で対処
			vertex.position.x *= -1.0f;
			vertex.normal.x *= -1.0f;
			modelData.vertices.push_back(vertex);
		}

		//面インデックスを解析して配列に突っ込む
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)//face解析
		{
			aiFace& face = mesh->mFaces[faceIndex];
			if (face.mNumIndices != 3) {
				continue;
			}
			for (uint32_t element = 0; element < face.mNumIndices; ++element)//vertex解析
			{
				uint32_t vertexIndex = face.mIndices[element];
				//オフセットを足してインデックスを保存する
				modelData.indices.push_back(vertexIndex + vertexOffset);
			}
		}
		meshData.indexCount = static_cast<uint32_t>(modelData.indices.size()) - meshData.indexOffset;
		if (meshData.indexCount != 0)
		{
			modelData.meshes.push_back(meshData);
		}

		//SkinCluster
		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
		{
			//Jointごとに格納領域を作る
			aiBone* bone = mesh->mBones[boneIndex];
			std::string jointName = bone->mName.C_Str();
			JointWeightData& jointWeightData = modelData.skinClusterData[jointName];

			aiMatrix4x4 bindPoseMatrixAssimp = bone->mOffsetMatrix.Inverse();//BindPoseMatrixに戻す
			aiVector3D scale, translate;
			aiQuaternion rotate;
			bindPoseMatrixAssimp.Decompose(scale, rotate, translate);//成分を抽出

			//左手系のBindPoseMatrixを作る
			Matrix4x4 bindPoseMatrix = MakeAffineMatrixQuaternion(
				{ scale.x,scale.y,scale.z }, { rotate.x,-rotate.y,-rotate.z,rotate.w }, { -translate.x,translate.y,translate.z });

			//InverseBindPoseMatrixにする
			jointWeightData.inverseBindPoseMatrix = Inverse(bindPoseMatrix);

			//Weight情報を取り出す
			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
			{
				jointWeightData.vertexWeights.push_back({ bone->mWeights[weightIndex].mWeight,bone->mWeights[weightIndex].mVertexId + vertexOffset });
			}
		}
	}
	return !modelData.vertices.empty() && !modelData.indices.empty() && !modelData.meshes.empty();
}
