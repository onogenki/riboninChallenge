#include "Mirror.hlsli"

struct ReflectionData
{
    float32_t4x4 reflectionViewProjection;
    float32_t4 tint;
};

ConstantBuffer<ReflectionData> gReflection : register(b0);
Texture2D<float32_t4> gReflectionTexture : register(t0);
SamplerState gSampler : register(s0);

float32_t4 main(MirrorVertexShaderOutput input) : SV_TARGET0
{
    float32_t4 reflectionClip = mul(
        float32_t4(input.worldPosition, 1.0f),
        gReflection.reflectionViewProjection);
    if (reflectionClip.w <= 0.0001f)
    {
        return float32_t4(0.05f, 0.07f, 0.09f, 1.0f);
    }

    float32_t2 projection = reflectionClip.xy / reflectionClip.w;
    float32_t2 uv = float32_t2(
        projection.x * 0.5f + 0.5f,
        -projection.y * 0.5f + 0.5f);
    if (any(uv < 0.0f) || any(uv > 1.0f))
    {
        return float32_t4(0.05f, 0.07f, 0.09f, 1.0f);
    }

    return gReflectionTexture.Sample(gSampler, uv) * gReflection.tint;
}
