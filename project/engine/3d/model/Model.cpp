#include "Model.h"
#include "DirectXCommon.h"
#include "Logger.h"
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

namespace Engine
{

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

// モデル本体と材質画像を順に準備し、画像を読めないモデルは登録させないようにします。
bool Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename)
{
	// 前回の途中資源を残さず、読込失敗時も未完成のModelを登録しないようにします。
	ResetGpuResources();
	if (!modelCommon || !modelCommon->GetDxCommon() ||
		!modelCommon->GetDxCommon()->GetDevice()) {
		Logger::Log("Model: ModelCommon or DirectX device is unavailable: " + filename);
		return false;
	}
	//ModelCommonのポインタを引数からメンバ変数に記録する
	modelCommon_ = modelCommon;
	//モデル読み込み
	if (!LoadModelFile(directoryPath, filename)) {
		return false;
	}

	// GPUへ渡す三つのBufferが揃ってから、材質画像の読込へ進みます。
	if (!CreateIndexData() || !CreateVertexData() || !CreateMaterialData()) {
		ResetGpuResources();
		return false;
	}
	for (MaterialData& material : modelData_.materials)
	{
		const std::filesystem::path texturePath = material.textureFilePath;
		// 空・None・先頭が*の画像参照は、従来どおり確認用画像へ置き換えます。
		// この画像が見えても、元画像の表示成功とは扱いません。
		if (material.textureFilePath.empty() ||
			material.textureFilePath.find("None") != std::string::npos ||
			material.textureFilePath.front() == '*')
		{
			material.textureFilePath = "Resources/debug/uvChecker.png";
		} else {
			// 指定された画像の欠損は代替画像で隠さず、モデルの読込失敗として伝えます。
			std::error_code error;
			if (!std::filesystem::exists(texturePath, error)) {
				Logger::Log("Model: referenced texture unavailable: " + material.textureFilePath);
				ResetGpuResources();
				return false;
			}
		}
		if (!TextureManager::GetInstance()->LoadTexture(material.textureFilePath)) {
			Logger::Log("Model: texture load failed: " + material.textureFilePath);
			ResetGpuResources();
			return false;
		}
	}
	return true;
}

// 通常モデルの頂点・材質Bufferを設定し、各Meshに対応するTextureで描画します。
void Model::Draw(uint32_t textureSrvIndexOverride)
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();

	// 1. 頂点バッファビューを設定 (通常のVBVは1つだけ)
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// インデックスバッファをセット
	commandList->IASetIndexBuffer(&indexBufferView_);

	// 2. 形状を設定 (三角形リスト)
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 3. マテリアルCBufferの設定 (RootParameter 0)
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

// 反射など別の描画設定を使う経路へ、形状だけを同じ順番で渡します。
void Model::DrawGeometry()
{
	ID3D12GraphicsCommandList* commandList = modelCommon_->GetDxCommon()->GetCommandList();
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	for (const MeshData& mesh : modelData_.meshes) {
		commandList->DrawIndexedInstanced(mesh.indexCount, 1, mesh.indexOffset, 0, 0);
	}
}

// 画像を読み込めた時だけ全材質を差し替え、再読込時にも使える明示指定として記録します。
void Model::SetTexture(const std::string& filePath)
{
	if (!TextureManager::GetInstance()->LoadTexture(filePath)) {
		Logger::Log("Model: texture load failed: " + filePath);
		return;
	}
	for (MaterialData& material : modelData_.materials)
	{
		material.textureFilePath = filePath;
	}
	runtimeTextureOverride_ = filePath;
}

// ファイル由来の頂点・材質一覧は新しいまま、実行中に調整した材質値と明示画像だけを引き継ぎます。
bool Model::CopyRuntimeSettingsFrom(const Model& previous)
{
	if (!materialData_ || !previous.materialData_) {
		return false;
	}
	if (previous.runtimeTextureOverride_ &&
		!TextureManager::GetInstance()->Contains(*previous.runtimeTextureOverride_)) {
		return false;
	}

	materialData_->color = previous.materialData_->color;
	materialData_->enableLighting = previous.materialData_->enableLighting;
	materialData_->uvTransform = previous.materialData_->uvTransform;
	materialData_->shininess = previous.materialData_->shininess;
	materialData_->environmentCoefficient = previous.materialData_->environmentCoefficient;
	materialData_->specularIntensity = previous.materialData_->specularIntensity;
	if (previous.runtimeTextureOverride_) {
		for (MaterialData& material : modelData_.materials) {
			material.textureFilePath = *previous.runtimeTextureOverride_;
		}
		runtimeTextureOverride_ = previous.runtimeTextureOverride_;
	}
	return true;
}

//アニメーション適用
// IndexをGPUへ渡すBufferを作り、生成・Mapの失敗を上位へ伝えます。
bool Model::CreateIndexData()
{
	if (modelData_.indices.empty()) {
		Logger::Log("Model: index data is empty");
		return false;
	}
	// --- インデックスバッファの作成 ---
	indexResource_ = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * modelData_.indices.size());
	if (!indexResource_) {
		Logger::Log("Model: index buffer creation failed");
		return false;
	}

	// インデックスバッファビューの設定
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * static_cast<uint32_t>(modelData_.indices.size());
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	// --- データのマップとコピー ---
	uint32_t* mappedIndex = nullptr;
	const HRESULT mapResult = indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
	if (FAILED(mapResult) || !mappedIndex) {
		Logger::Log("Model: index buffer Map failed");
		return false;
	}
	std::memcpy(mappedIndex, modelData_.indices.data(), sizeof(uint32_t) * modelData_.indices.size());
	indexResource_->Unmap(0, nullptr);
	return true;
}

// 頂点Bufferが完成した時だけ、描画用のViewとMap先を有効にします。
bool Model::CreateVertexData()
{
	if (modelData_.vertices.empty()) {
		Logger::Log("Model: vertex data is empty");
		return false;
	}
	// 書き込むデータのサイズ（頂点データ全体のバイト数）
	size_t sizeInBytes = sizeof(VertexData) * modelData_.vertices.size();

	// 1. 頂点リソースを作る (Spriteから持ってきたヘルパー関数を使用)
	vertexResource_ = modelCommon_->GetDxCommon()->CreateBufferResource(sizeInBytes);
	if (!vertexResource_) {
		Logger::Log("Model: vertex buffer creation failed");
		return false;
	}

	// 2. 頂点バッファビューを作成
	// リソースの先頭のアドレスから使う
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	// 使用するリソース of サイズ
	vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeInBytes);
	// 1頂点あたりのサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// 3. データを書き込む
	// 書き込むためのアドレスを取得
	const HRESULT mapResult = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	if (FAILED(mapResult) || !vertexData_) {
		Logger::Log("Model: vertex buffer Map failed");
		return false;
	}

	// 読み込んだモデルデータ(std::vector)の中身を、マップしたメモリ(vertexData_)へ一括コピー
	std::memcpy(vertexData_, modelData_.vertices.data(), sizeInBytes);
	return true;
}

// Materialの初期値を書けるMap先が得られた時だけ成功とします。
bool Model::CreateMaterialData()
{
	// 1. マテリアルリソースを作る (サイズは Material 構造体1つ分)
	materialResource_ = modelCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));
	if (!materialResource_) {
		Logger::Log("Model: material buffer creation failed");
		return false;
	}

	// 2. データを書き込むためのアドレスを取得して materialData_ に割り当てる
	const HRESULT mapResult = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	if (FAILED(mapResult) || !materialData_) {
		Logger::Log("Model: material buffer Map failed");
		return false;
	}

	// 3. マテリアルデータの初期値を書き込む
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	materialData_->enableLighting = true;

	materialData_->shininess = 50.0f;

	materialData_->environmentCoefficient = 0.0f;
	// 通常のModelはLightの反射をそのまま使います。
	materialData_->specularIntensity = 1.0f;

	materialData_->uvTransform = MakeIdentity4x4();
	return true;
}

// Modelが所有するBufferとCPUのMap先を一緒に無効にし、失敗後の再利用を防ぎます。
void Model::ResetGpuResources()
{
	if (vertexResource_ && vertexData_) {
		vertexResource_->Unmap(0, nullptr);
	}
	if (materialResource_ && materialData_) {
		materialResource_->Unmap(0, nullptr);
	}
	vertexData_ = nullptr;
	materialData_ = nullptr;
	indexBufferView_ = {};
	vertexBufferView_ = {};
	indexResource_.Reset();
	vertexResource_.Reset();
	materialResource_.Reset();
}

// モデルファイルから頂点・材質・骨情報を読み、GPU Buffer生成前のCPUデータをそろえます。
bool Model::LoadModelFile(const std::string& directoryPath, const std::string& filename)
{
	// メンバ変数をクリアしておく
	modelData_.vertices.clear();
	modelData_.indices.clear();
	modelData_.materials.clear();
	modelData_.meshes.clear();

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

	modelData_.rootNode = ReadNode(scene->mRootNode);
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
		modelData_.materials.push_back(std::move(materialData));
	}
	if (modelData_.materials.empty())
	{
		modelData_.materials.push_back({});
	}

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)//mesh解析
	{
		aiMesh* mesh = scene->mMeshes[meshIndex];
		//現在の頂点数を保存しておく(複数メッシュ対応のため)
		if (!mesh->HasPositions() || !mesh->HasFaces()) {
			continue;
		}
		uint32_t vertexOffset = static_cast<uint32_t>(modelData_.vertices.size());
		MeshData meshData{};
		meshData.indexOffset = static_cast<uint32_t>(modelData_.indices.size());
		meshData.materialIndex = (std::min)(mesh->mMaterialIndex, static_cast<uint32_t>(modelData_.materials.size() - 1));

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
			modelData_.vertices.push_back(vertex);
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
				modelData_.indices.push_back(vertexIndex + vertexOffset);
			}
		}
		meshData.indexCount = static_cast<uint32_t>(modelData_.indices.size()) - meshData.indexOffset;
		if (meshData.indexCount != 0)
		{
			modelData_.meshes.push_back(meshData);
		}

		//SkinCluster
		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
		{
			//Jointごとに格納領域を作る
			aiBone* bone = mesh->mBones[boneIndex];
			std::string jointName = bone->mName.C_Str();
			JointWeightData& jointWeightData = modelData_.skinClusterData[jointName];

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
	return !modelData_.vertices.empty() && !modelData_.indices.empty() && !modelData_.meshes.empty();
}

}
