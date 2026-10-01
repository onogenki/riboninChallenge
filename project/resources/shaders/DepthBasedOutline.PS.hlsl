#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);

struct Material
{
    float32_t4x4 projectionInverse;
};

ConstantBuffer<Material> gMaterial : register(b0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

static const float32_t kPrewittHorizontalKernel[3][3] = {
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
};

static const float32_t kPrewittVerticalKernel[3][3] = {
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f },
};

float32_t ViewSpaceDepth(float32_t ndcDepth)
{
    float32_t4 viewSpace = mul(float32_t4(0.0f, 0.0f, ndcDepth, 1.0f), gMaterial.projectionInverse);
    return viewSpace.z * rcp(viewSpace.w);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    uint32_t width;
    uint32_t height;
    gDepthTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(float32_t(width)), rcp(float32_t(height)));

    float32_t2 difference = float32_t2(0.0f, 0.0f);
    for (int32_t y = -1; y <= 1; ++y)
    {
        for (int32_t x = -1; x <= 1; ++x)
        {
            float32_t2 texcoord = input.texcoord + float32_t2(float32_t(x), float32_t(y)) * uvStepSize;
            float32_t viewSpaceDepth = ViewSpaceDepth(gDepthTexture.Sample(gSamplerPoint, texcoord));
            difference.x += viewSpaceDepth * kPrewittHorizontalKernel[y + 1][x + 1];
            difference.y += viewSpaceDepth * kPrewittVerticalKernel[y + 1][x + 1];
        }
    }

    float32_t weight = saturate(length(difference));

    PixelShaderOutput output;
    output.color.rgb = (1.0f - weight) * gTexture.Sample(gSampler, input.texcoord).rgb;
    output.color.a = 1.0f;
    return output;
}
