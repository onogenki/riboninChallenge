#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

static const float32_t kKernel5x5[5][5] = {
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
    { 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f, 1.0f / 25.0f },
};

PixelShaderOutput main(VertexShaderOutput input)
{
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(float32_t(width)), rcp(float32_t(height)));

    PixelShaderOutput output;
    output.color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    for (int32_t x = -2; x <= 2; ++x)
    {
        for (int32_t y = -2; y <= 2; ++y)
        {
            float32_t2 texcoord = input.texcoord + float32_t2(float32_t(x), float32_t(y)) * uvStepSize;
            output.color += gTexture.Sample(gSampler, texcoord) * kKernel5x5[x + 2][y + 2];
        }
    }

    return output;
}
