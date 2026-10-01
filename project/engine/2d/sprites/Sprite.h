#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Transform.h"
#include"MyMath.h"
#include "SpriteCommon.h"
#include <string>

class SpriteCommon;

//スプライト1枚分
class Sprite
{
public:

	//頂点データ
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	//マテリアルデータ
	struct Material
	{
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
		float shininess;
	};

	// 座標変換行列データ
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	Sprite() = default;
	~Sprite() = default;

	void Initialize(SpriteCommon* spriteCommon,std::string textureFilePath);

	void Update();

	void Draw();

	//getter
	const Vector2& GetPosition() const { return position; }//座標
	float GetRotation() const { return rotation; }//回転
	const Vector4& GetColor()const { return materialData->color; }//色
	const Vector2& GetSize() const { return size; }//サイズ
	const Vector2& GetAnchorPoint()const { return anchorPoint; }//アンカーポイント
	bool GetIsFlipX()const { return isFlipX_; }//フリップX
	bool GetIsFlipY()const { return isFlipY_; }//フリップY
	const Vector2& GetTextureLeftTop()const { return textureLeftTop; }
	const Vector2& GetTextureSize()const { return textureSize; }

	//setter
	void SetPosition(const Vector2& position) { this->position = position; }//座標
	void SetRotation(float rotation) { this->rotation = rotation; }//回転
	void SetColor(const Vector4& color) { materialData->color = color; }//色
	void SetSize(const Vector2& size) { this->size = size; }//サイズ
	void SetTexture(const std::string& textureFilePath);// テクスチャを変更する関数
	void SetAnchorPoint(const Vector2& anchorPoint) { this->anchorPoint = anchorPoint; }//アンカーポイント
	void SetIsFlipX(bool isFlipX) { this->isFlipX_ = isFlipX; }
	void SetIsFlipY(bool isFlipY) { this->isFlipY_ = isFlipY; }
	void SetTextureLeftTop(const Vector2& textureLeftTop) { this->textureLeftTop = textureLeftTop; }
	void SetTextureSize(const Vector2& textureSize) { this->textureSize = textureSize; }


	Transform& GetTransform() { return transform; }
private:

	SpriteCommon* spriteCommon = nullptr;

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource>vertexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>indexResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>materialResource;
	Microsoft::WRL::ComPtr<ID3D12Resource>transformationMatrixResource;

	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};


	//バッファリソース内のデータを指すポインタ
	VertexData* vertexData = nullptr;
	uint32_t* indexData = nullptr;
	Material* materialData = nullptr;
	TransformationMatrix* transformationMatrixData = nullptr;

	Vector2 position = { 0.0f,0.0f };

	float rotation = 0.0f;

	Vector2 size = { 640.0f,360.0f };

	Transform transform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	Transform uvTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	//テクスチャ番号
	//uint32_t textureIndex_ = 0;
	std::string textureFilePath_;

	//基準点
	Vector2 anchorPoint = { 0.0f,0.0f };

	//左右フリップ
	bool isFlipX_ = false;
	//上下フリップ
	bool isFlipY_ = false;

	//テクスチャ左上座標
	Vector2 textureLeftTop = { 0.0f,0.0f };

	//テクスチャ切り出しサイズ
	Vector2 textureSize = { 100.0f,100.0f };

	//テクスチャサイズをイメージに合わせる
	void AdjustTextureSize();
};
