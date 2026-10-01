#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

static const float32_t kPi = 3.14159265f;

float32_t gauss(float32_t x, float32_t sigma)
{
    float32_t exponent = -(x * x) * rcp(2.0f * sigma * sigma);
    float32_t denominator = sqrt(2.0f * kPi) * sigma;
    return exp(exponent) * rcp(denominator);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(float32_t(width)), rcp(float32_t(height)));

    PixelShaderOutput output;
    output.color = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);

    float32_t weight = 0.0f;
    for (int32_t x = -3; x <= 3; x += 2)
    {
        float32_t firstKernel = gauss(float32_t(x), 2.0f);
        float32_t secondKernel = x < 3 ? gauss(float32_t(x + 1), 2.0f) : 0.0f;
        float32_t combinedKernel = firstKernel + secondKernel;
        float32_t offset = float32_t(x) + secondKernel * rcp(combinedKernel);
        float32_t2 texcoord = input.texcoord + float32_t2(offset, 0.0f) * uvStepSize;
        output.color += gTexture.Sample(gSampler, texcoord) * combinedKernel;
        weight += combinedKernel;
    }
    output.color *= rcp(weight);

    return output;
}
