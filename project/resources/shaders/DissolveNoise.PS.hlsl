#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t4> gMaskTexture : register(t1);
SamplerState gSampler : register(s0);

struct DissolveData
{
    float32_t threshold;
    float32_t edgeWidth;
    float32_t2 padding;
    float32_t4 edgeColor;
};

ConstantBuffer<DissolveData> gDissolveData : register(b1);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t mask = gMaskTexture.Sample(gSampler, input.texcoord).r;
    if (mask < gDissolveData.threshold)
    {
        discard;
    }

    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);
    return output;
}
