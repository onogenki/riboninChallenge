#include "Sprite.hlsli"

// スプライト頂点をクリップ空間へ移し、UVと法線をピクセルシェーダーへ渡す。
// 座標変換行列 (b0)
struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// 頂点データ
struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // WVP行列を使って画面上の正しい位置に変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = input.normal;
    
    return output;
}
