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

// 1枚の2D画像の位置・色・切り出し範囲と、その描画用GPUデータを所有します。
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

	// 共通描画部品を借り、四つのBufferと画像がそろった時だけtrueを返します。
	bool Initialize(SpriteCommon* spriteCommon, const std::string& textureFilePath);

	// 現在の位置・サイズ・切り出し設定から、行列と頂点の値を更新します。
	void Update();

	void Draw();

	//getter
	const Vector2& GetPosition() const { return position_; }//座標
	float GetRotation() const { return rotation_; }//回転
	const Vector4& GetColor()const { return materialData_->color; }//色
	const Vector2& GetSize() const { return size_; }//サイズ
	const Vector2& GetAnchorPoint()const { return anchorPoint_; }//アンカーポイント
	bool GetIsFlipX()const { return isFlipX_; }//フリップX
	bool GetIsFlipY()const { return isFlipY_; }//フリップY
	const Vector2& GetTextureLeftTop()const { return textureLeftTop_; }
	const Vector2& GetTextureSize()const { return textureSize_; }

	//setter
	void SetPosition(const Vector2& position) { position_ = position; }//座標
	void SetRotation(float rotation) { rotation_ = rotation; }//回転
	void SetColor(const Vector4& color) { materialData_->color = color; }//色
	void SetSize(const Vector2& size) { size_ = size; }//サイズ
	// 読込失敗時は現在の画像を維持し、変更しなかったことを返します。
	bool SetTexture(const std::string& textureFilePath);
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }//アンカーポイント
	void SetIsFlipX(bool isFlipX) { this->isFlipX_ = isFlipX; }
	void SetIsFlipY(bool isFlipY) { this->isFlipY_ = isFlipY; }
	void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }
	void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }


	// 既存の編集APIを維持し、Spriteの変換値を呼び出し元へ渡します。
	Transform& GetTransform() { return transform_; }
private:

	SpriteCommon* spriteCommon_ = nullptr;

	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource>vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource>indexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource>materialResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource>transformationMatrixResource_;

	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};


	//バッファリソース内のデータを指すポインタ
	VertexData* vertexData_ = nullptr;
	uint32_t* indexData_ = nullptr;
	Material* materialData_ = nullptr;
	TransformationMatrix* transformationMatrixData_ = nullptr;

	Vector2 position_ = { 0.0f,0.0f };

	float rotation_ = 0.0f;

	Vector2 size_ = { 640.0f,360.0f };

	Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	Transform uvTransform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	std::string textureFilePath_;

	//基準点
	Vector2 anchorPoint_ = { 0.0f,0.0f };

	//左右フリップ
	bool isFlipX_ = false;
	//上下フリップ
	bool isFlipY_ = false;

	//テクスチャ左上座標
	Vector2 textureLeftTop_ = { 0.0f,0.0f };

	//テクスチャ切り出しサイズ
	Vector2 textureSize_ = { 100.0f,100.0f };

	//テクスチャサイズをイメージに合わせる
	void AdjustTextureSize();
	// 途中生成したGPU資源と、それを指すCPUポインタを一緒に外す。
	void ResetGpuResources();
};
