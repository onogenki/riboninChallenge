#include "Sprite.h"
#include"SpriteCommon.h"
#include "MyMath.h"
#include "Logger.h"
#include "TextureManager.h"

using namespace MyMath;

void Sprite::AdjustTextureSize()
{
	//テクスチャメタデータを取得
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureFilePath_);

	textureSize.x = static_cast<float>(metadata.width);
	textureSize.y = static_cast<float>(metadata.height);

	//画像サイズをテクスチャサイズに合わせる
	size = textureSize;

}

// 四つのBufferを従来の順に準備し、失敗したSpriteを描画一覧へ渡しません。
bool Sprite::Initialize(SpriteCommon* spriteCommon, const std::string& textureFilePath)
{
	ResetGpuResources();
	if (!spriteCommon || !spriteCommon->GetDxCommon() || !spriteCommon->GetDevice()) {
		Logger::Log("Sprite initialization failed: shared drawing resources are unavailable.");
		return false;
	}
	const auto fail = [this](const char* resourceName) {
		Logger::Log(std::string("Sprite initialization failed: ") + resourceName + " buffer creation or mapping failed.");
		ResetGpuResources();
		return false;
	};
	position = { 0.0f, 0.0f };
	rotation = 0.0f;
	anchorPoint = { 0.0f, 0.0f }; // 左上を基準（または0.5fで中心）
	isFlipX_ = false;
	isFlipY_ = false;
	textureLeftTop = { 0.0f, 0.0f };

	//引数で受け取ってメンバ変数に記録する
	this->spriteCommon = spriteCommon;

	//頂点リソースを作る
	vertexResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * 4);
	if (!vertexResource) {
		return fail("vertex");
	}

	//頂点リソースの先頭のGPUアドレスを描画に使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	//四角形を共有頂点4つで表すため、その4つ分のサイズを指定する
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 4;
	//1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	//書き込むためのアドレスを取得
	if (FAILED(vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData))) || !vertexData) {
		return fail("vertex");
	}

	//各頂点の法線を初期化します。座標とUVはUpdateでサイズに合わせて更新します。
	vertexData[0].normal = { 0.0f,0.0f,-1.0f };
	vertexData[1].normal = { 0.0f,0.0f,-1.0f };
	vertexData[2].normal = { 0.0f,0.0f,-1.0f };
	vertexData[3].normal = { 0.0f,0.0f,-1.0f };

	indexResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * 6);
	if (!indexResource) {
		return fail("index");
	}

	//リソースの先頭のアドレスから使う
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferView.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	if (FAILED(indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData))) || !indexData) {
		return fail("index");
	}
	indexData[0] = 0;  indexData[1] = 1;   indexData[2] = 2;
	indexData[3] = 1;  indexData[4] = 3;   indexData[5] = 2;

	//色とUV変換を保持するMaterial全体のリソースを作る
	materialResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(Material));
	if (!materialResource) {
		return fail("material");
	}
	//書き込むためのアドレスを取得
	if (FAILED(materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData))) || !materialData) {
		return fail("material");
	}

	//Texture本来の色を使えるよう、乗算色を白にしておく
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = false;
	materialData->uvTransform = MakeIdentity4x4();

	//TransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	transformationMatrixResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
	if (!transformationMatrixResource) {
		return fail("transformation matrix");
	}
	//座標変換行列リソースにデータを書き込むためのアドレスを取得してtransformationMatrixDataに割り当てる
	if (FAILED(transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData))) ||
		!transformationMatrixData) {
		return fail("transformation matrix");
	}

	// 単位行列で初期化
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();


	transform.translate = { position.x,position.y,0.0f };


	uvTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };


	if (!SetTexture(textureFilePath)) {
		ResetGpuResources();
		return false;
	}
	return true;
}

// Map済みポインタを使えない状態へ戻し、対応するBufferも解放します。
void Sprite::ResetGpuResources()
{
	vertexData = nullptr;
	indexData = nullptr;
	materialData = nullptr;
	transformationMatrixData = nullptr;
	vertexResource.Reset();
	indexResource.Reset();
	materialResource.Reset();
	transformationMatrixResource.Reset();
	vertexBufferView = {};
	indexBufferView = {};
	textureFilePath_.clear();
	spriteCommon = nullptr;
}

void Sprite::Update()
{
	transform.translate = { position.x, position.y, 0.0f };
	transform.rotate = { 0.0f,0.0f,rotation };
	transform.scale = { size.x,size.y,1.0f };

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 viewMatrix = MakeIdentity4x4();
	const float clientWidth = static_cast<float>(spriteCommon->GetDxCommon()->GetClientWidth());
	const float clientHeight = static_cast<float>(spriteCommon->GetDxCommon()->GetClientHeight());
	Matrix4x4 projectionMatrix = MakeOrthographicMatrix(0.0f, 0.0f, clientWidth, clientHeight, 0.0f, 100.0f);
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	transformationMatrixData->WVP = worldViewProjectionMatrix;
	transformationMatrixData->World = worldMatrix;

	Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransform.scale);
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransform.rotate.z));
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransform.translate));
	materialData->uvTransform = uvTransformMatrix;

	float left = 0.0f - anchorPoint.x;
	float right = 1.0f - anchorPoint.x;
	float top = 0.0f - anchorPoint.y;
	float bottom = 1.0f - anchorPoint.y;

	//左右反転
	if (isFlipX_)
	{
		left = -left;
		right = -right;
	}
	//上下反転
	if (isFlipY_)
	{
		top = -top;
		bottom = -bottom;
	}

	const DirectX::TexMetadata& metadata =
		TextureManager::GetInstance()->GetMetaData(textureFilePath_);
	float tex_left = textureLeftTop.x / metadata.width;
	float tex_right = (textureLeftTop.x + textureSize.x) / metadata.width;
	float tex_top = textureLeftTop.y / metadata.height;
	float tex_bottom = (textureLeftTop.y + textureSize.y) / metadata.height;

	vertexData[0].position = { left,bottom,0.0f,1.0f };//左下
	vertexData[1].position = { left,top,0.0f,1.0f };//左上
	vertexData[2].position = { right,bottom,0.0f,1.0f };//右下
	vertexData[3].position = { right,top,0.0f,1.0f };//右上

	vertexData[0].texcoord = { tex_left, tex_bottom };//左下
	vertexData[1].texcoord = { tex_left, tex_top };//左上
	vertexData[2].texcoord = { tex_right,tex_bottom };//右下
	vertexData[3].texcoord = { tex_right,tex_top };//右上

}

void Sprite::Draw()
{
	ID3D12GraphicsCommandList* commandList = spriteCommon->GetCommandList();
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->IASetIndexBuffer(&indexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetSrvHandleGPU(textureFilePath_));
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

}

// 読めない画像なら現在のSprite設定を変えず、呼び出し元へ失敗を返します。
bool Sprite::SetTexture(const std::string& textureFilePath) {
	// Loadingなど、Sceneが事前読込していない画像も、サイズを参照する前に必ず読み込みます。
	// 同じ画像の再読込はTextureManagerが省くため、Sprite側でGPU資源を重複所有しません。
	const bool isLoaded = TextureManager::GetInstance()->LoadTexture(textureFilePath);
	if (!isLoaded) {
		Logger::Log("Sprite texture could not be loaded: " + textureFilePath);
		return false;
	}
	//TextureManagerから、その画像のインデックス番号を取得
	textureFilePath_ = textureFilePath;

	// 切り出し位置もリセットしておく（左上に戻す）
	textureLeftTop = { 0.0f, 0.0f };

	//サイズも表示範囲も自動的に画像に合わせる
	AdjustTextureSize();
	return true;
}
