struct LaserData
{
    float32_t4x4 viewProjection;
    float32_t4 color;
};

ConstantBuffer<LaserData> gLaser : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
};

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gLaser.viewProjection);
    // 帯の左右端と長さ方向の位置をPixelShaderへ渡し、透明な縁を計算できるようにします。
    output.texcoord = input.texcoord;
    return output;
}
