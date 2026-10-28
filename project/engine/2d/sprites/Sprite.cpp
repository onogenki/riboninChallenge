#include "Sprite.h"
#include"SpriteCommon.h"
#include "MyMath.h"
#include "Logger.h"
#include "TextureManager.h"

using namespace MyMath;

namespace Engine
{

// 読み込んだ画像の寸法を切り出し範囲と表示サイズへ揃え、初期表示を元画像どおりにします。
void Sprite::AdjustTextureSize()
{
	//テクスチャメタデータを取得
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(textureFilePath_);

	textureSize_.x = static_cast<float>(metadata.width);
	textureSize_.y = static_cast<float>(metadata.height);

	//画像サイズをテクスチャサイズに合わせる
	size_ = textureSize_;

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
	position_ = { 0.0f, 0.0f };
	rotation_ = 0.0f;
	anchorPoint_ = { 0.0f, 0.0f }; // 左上を基準（または0.5fで中心）
	isFlipX_ = false;
	isFlipY_ = false;
	textureLeftTop_ = { 0.0f, 0.0f };

	//引数で受け取ってメンバ変数に記録する
	this->spriteCommon_ = spriteCommon;

	//頂点リソースを作る
	vertexResource_ = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * 4);
	if (!vertexResource_) {
		return fail("vertex");
	}

	//頂点リソースの先頭のGPUアドレスを描画に使う
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	//四角形を共有頂点4つで表すため、その4つ分のサイズを指定する
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
	//1頂点あたりのサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	//書き込むためのアドレスを取得
	if (FAILED(vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_))) || !vertexData_) {
		return fail("vertex");
	}

	//各頂点の法線を初期化します。座標とUVはUpdateでサイズに合わせて更新します。
	vertexData_[0].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[1].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[2].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[3].normal = { 0.0f,0.0f,-1.0f };

	indexResource_ = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * 6);
	if (!indexResource_) {
		return fail("index");
	}

	//リソースの先頭のアドレスから使う
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	if (FAILED(indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_))) || !indexData_) {
		return fail("index");
	}
	indexData_[0] = 0;  indexData_[1] = 1;   indexData_[2] = 2;
	indexData_[3] = 1;  indexData_[4] = 3;   indexData_[5] = 2;

	//色とUV変換を保持するMaterial全体のリソースを作る
	materialResource_ = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(Material));
	if (!materialResource_) {
		return fail("material");
	}
	//書き込むためのアドレスを取得
	if (FAILED(materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_))) || !materialData_) {
		return fail("material");
	}

	//Texture本来の色を使えるよう、乗算色を白にしておく
	materialData_->color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialData_->enableLighting = false;
	materialData_->uvTransform = MakeIdentity4x4();

	//TransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	transformationMatrixResource_ = spriteCommon->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
	if (!transformationMatrixResource_) {
		return fail("transformation matrix");
	}
	//座標変換行列リソースにデータを書き込むためのアドレスを取得してtransformationMatrixData_に割り当てる
	if (FAILED(transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_))) ||
		!transformationMatrixData_) {
		return fail("transformation matrix");
	}

	// 単位行列で初期化
	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();


	transform_.translate = { position_.x,position_.y,0.0f };


	uvTransform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };


	if (!SetTexture(textureFilePath)) {
		ResetGpuResources();
		return false;
	}
	return true;
}

// Map済みポインタを使えない状態へ戻し、対応するBufferも解放します。
void Sprite::ResetGpuResources()
{
	vertexData_ = nullptr;
	indexData_ = nullptr;
	materialData_ = nullptr;
	transformationMatrixData_ = nullptr;
	vertexResource_.Reset();
	indexResource_.Reset();
	materialResource_.Reset();
	transformationMatrixResource_.Reset();
	vertexBufferView_ = {};
	indexBufferView_ = {};
	textureFilePath_.clear();
	spriteCommon_ = nullptr;
}

// 位置・回転・切り出し範囲をGPU用の行列と頂点へ反映し、描画前の状態を揃えます。
void Sprite::Update()
{
	transform_.translate = { position_.x, position_.y, 0.0f };
	transform_.rotate = { 0.0f,0.0f,rotation_ };
	transform_.scale = { size_.x,size_.y,1.0f };

	Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	Matrix4x4 viewMatrix = MakeIdentity4x4();
	const float clientWidth = static_cast<float>(spriteCommon_->GetDxCommon()->GetClientWidth());
	const float clientHeight = static_cast<float>(spriteCommon_->GetDxCommon()->GetClientHeight());
	Matrix4x4 projectionMatrix = MakeOrthographicMatrix(0.0f, 0.0f, clientWidth, clientHeight, 0.0f, 100.0f);
	Matrix4x4 worldViewProjectionMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	transformationMatrixData_->WVP = worldViewProjectionMatrix;
	transformationMatrixData_->World = worldMatrix;

	Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransform_.scale);
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransform_.rotate.z));
	uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransform_.translate));
	materialData_->uvTransform = uvTransformMatrix;

	float left = 0.0f - anchorPoint_.x;
	float right = 1.0f - anchorPoint_.x;
	float top = 0.0f - anchorPoint_.y;
	float bottom = 1.0f - anchorPoint_.y;

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
	float tex_left = textureLeftTop_.x / metadata.width;
	float tex_right = (textureLeftTop_.x + textureSize_.x) / metadata.width;
	float tex_top = textureLeftTop_.y / metadata.height;
	float tex_bottom = (textureLeftTop_.y + textureSize_.y) / metadata.height;

	vertexData_[0].position = { left,bottom,0.0f,1.0f };//左下
	vertexData_[1].position = { left,top,0.0f,1.0f };//左上
	vertexData_[2].position = { right,bottom,0.0f,1.0f };//右下
	vertexData_[3].position = { right,top,0.0f,1.0f };//右上

	vertexData_[0].texcoord = { tex_left, tex_bottom };//左下
	vertexData_[1].texcoord = { tex_left, tex_top };//左上
	vertexData_[2].texcoord = { tex_right,tex_bottom };//右下
	vertexData_[3].texcoord = { tex_right,tex_top };//右上

}

// 更新済みの頂点と画像を共通パイプラインへ渡し、一枚のSpriteを描きます。
void Sprite::Draw()
{
	ID3D12GraphicsCommandList* commandList = spriteCommon_->GetCommandList();
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
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
	textureLeftTop_ = { 0.0f, 0.0f };

	//サイズも表示範囲も自動的に画像に合わせる
	AdjustTextureSize();
	return true;
}

} // namespace Engine
