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

// @brief 1枚の2D画像の位置・色・切り出し範囲と、その描画用GPUデータを所有します。
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

	// @brief 共通描画部品を借り、四つのBufferと画像を描画前にそろえます。
	// @param spriteCommon 初期化済みの共通描画部品です。Spriteは所有しません。
	// @param textureFilePath 最初に表示する画像のパスです。
	// @return Bufferと画像の準備がすべて成功した時だけtrueです。
	bool Initialize(SpriteCommon* spriteCommon, const std::string& textureFilePath);

	// 現在の位置・サイズ・切り出し設定から、行列と頂点の値を更新します。
	void Update();

	// Updateで反映した頂点・行列・画像を使って、一枚のSpriteを描画します。
	void Draw();

	// 読み取り専用の表示設定を返します。色の取得にはInitializeの成功が必要です。
	const Vector2& GetPosition() const { return position_; }//座標
	float GetRotation() const { return rotation_; }//回転
	const Vector4& GetColor()const { return materialData_->color; }//色
	const Vector2& GetSize() const { return size_; }//サイズ
	const Vector2& GetAnchorPoint()const { return anchorPoint_; }//アンカーポイント
	bool GetIsFlipX()const { return isFlipX_; }//フリップX
	bool GetIsFlipY()const { return isFlipY_; }//フリップY
	const Vector2& GetTextureLeftTop()const { return textureLeftTop_; }
	const Vector2& GetTextureSize()const { return textureSize_; }

	// 位置・回転・サイズは次のUpdateで描画行列へ反映します。
	// @param position anchorPointで指定した基準点の表示座標です。
	void SetPosition(const Vector2& position) { position_ = position; }//座標
	// @param rotation Z軸まわりの回転角です。
	void SetRotation(float rotation) { rotation_ = rotation; }//回転
	// @param color 画像へ乗算するRGBA色です。Initialize成功後に指定します。
	void SetColor(const Vector4& color) { materialData_->color = color; }//色
	// @param size 表示する幅と高さです。
	void SetSize(const Vector2& size) { size_ = size; }//サイズ
	// @brief 読込成功時に画像・切り出し位置・表示サイズを更新します。
	// @param textureFilePath 新しく表示する画像のパスです。
	// @return 読込成功時だけtrueです。失敗時は現在の画像と表示設定を保ちます。
	bool SetTexture(const std::string& textureFilePath);
	// @param anchorPoint 四角形内の基準位置です。{0, 0}は左上、{0.5, 0.5}は中央です。
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }//アンカーポイント
	// @param isFlipX trueなら表示する画像を左右反転します。
	void SetIsFlipX(bool isFlipX) { this->isFlipX_ = isFlipX; }
	// @param isFlipY trueなら表示する画像を上下反転します。
	void SetIsFlipY(bool isFlipY) { this->isFlipY_ = isFlipY; }
	// @param textureLeftTop 元画像内で切り出す左上座標です。
	void SetTextureLeftTop(const Vector2& textureLeftTop) { textureLeftTop_ = textureLeftTop; }
	// @param textureSize 元画像内で切り出す幅と高さです。
	void SetTextureSize(const Vector2& textureSize) { textureSize_ = textureSize; }


	// @brief 既存APIとして内部Transformを返します。次のUpdateで位置・回転・サイズから上書きされます。
	// @return Spriteが所有するTransformへの参照です。継続的な変更には各Set関数を使います。
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
