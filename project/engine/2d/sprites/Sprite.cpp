#include "Sprite.h"
#include"SpriteCommon.h"
#include "MyMath.h"
#include <cassert>
#include "Model.h"
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

void Sprite::Initialize(SpriteCommon* spriteCommon, std::string textureFilePath)
{	 
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

	//リリースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点3つ分のサイズ
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 4;
	//1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	//書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	//1枚目の三角形
	//左下
	//vertexData[0].position = { 0.0f,1.0f,0.0f,1.0f };
	//vertexData[0].texcoord = { 0.0f,1.0f };
	vertexData[0].normal = { 0.0f,0.0f,-1.0f };
	//左上
	//vertexData[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	//vertexData[1].texcoord = { 0.0f,0.0f };
	vertexData[1].normal = { 0.0f,0.0f,-1.0f };
	//右下
	//vertexData[2].position = { 1.0f,1.0f,0.0f,1.0f };
	//vertexData[2].texcoord = { 1.0f,1.0f };
	vertexData[2].normal = { 0.0f,0.0f,-1.0f };
	//右上
	//vertexData[3].position = { 1.0f, 0.0f, 0.0f, 1.0f };
	//vertexData[3].texcoord = { 1.0f,0.0f };
	vertexData[3].normal = { 0.0f,0.0f,-1.0f };

	indexResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * 6);

	//リソースの先頭のアドレスから使う
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferView.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	indexData[0] = 0;  indexData[1] = 1;   indexData[2] = 2;
	indexData[3] = 1;  indexData[4] = 3;   indexData[5] = 2;

	//マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	materialResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(Material));
	//書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	//今回は赤を書き込んでみる
	materialData->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData->enableLighting = false;
	materialData->uvTransform = MakeIdentity4x4();

	//TransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	transformationMatrixResource = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
	//座標変換行列リソースにデータを書き込むためのアドレスを取得してtransformationMatrixDataに割り当てる
	transformationMatrixResource->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData));

	// 単位行列で初期化
	transformationMatrixData->WVP = MakeIdentity4x4();
	transformationMatrixData->World = MakeIdentity4x4();


	transform.translate = { position.x,position.y,0.0f };


	uvTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };


	//単位行列を書き込んでおく
	//textureIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);

	SetTexture(textureFilePath);
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

//画像をセットする関数
void Sprite::SetTexture(const std::string& textureFilePath) {
	// Loadingなど、Sceneが事前読込していない画像も、サイズを参照する前に必ず読み込みます。
	// 同じ画像の再読込はTextureManagerが省くため、Sprite側でGPU資源を重複所有しません。
	const bool isLoaded = TextureManager::GetInstance()->LoadTexture(textureFilePath);
	assert(isLoaded);
	if (!isLoaded) {
		return;
	}
	//TextureManagerから、その画像のインデックス番号を取得
	textureFilePath_ = textureFilePath;

	// 切り出し位置もリセットしておく（左上に戻す）
	textureLeftTop = { 0.0f, 0.0f };

	//サイズも表示範囲も自動的に画像に合わせる
	AdjustTextureSize();
}
