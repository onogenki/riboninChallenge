#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

static const float32_t2 kCenter = float32_t2(0.5f, 0.5f);
static const int32_t kNumSamples = 10;
static const float32_t kBlurWidth = 0.01f;

PixelShaderOutput main(VertexShaderOutput input)
{
    float32_t2 direction = input.texcoord - kCenter;
    float32_t3 outputColor = float32_t3(0.0f, 0.0f, 0.0f);
    for (int32_t sampleIndex = 0; sampleIndex < kNumSamples; ++sampleIndex)
    {
        float32_t2 texcoord = input.texcoord - direction * kBlurWidth * float32_t(sampleIndex);
        outputColor.rgb += gTexture.Sample(gSampler, texcoord).rgb;
    }

    PixelShaderOutput output;
    output.color.rgb = outputColor * rcp(float32_t(kNumSamples));
    output.color.a = 1.0f;
    return output;
}
