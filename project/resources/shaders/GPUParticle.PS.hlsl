struct PixelShaderInput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
    uint32_t emitterType : EMITTERTYPE0;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

float32_t4 main(PixelShaderInput input) : SV_TARGET0
{
    float light = 0.3f + saturate(dot(float32_t3(0.0f, 1.0f, 0.0f), -normalize(gDirectionalLight.direction))) * gDirectionalLight.intensity;
    float alpha = gTexture.Sample(gSampler, input.texcoord).a;
    if (input.emitterType == 1)
    {
        alpha = 1.0f;
    }
    else if (input.emitterType == 2)
    {
        alpha = step(abs(input.texcoord.x - 0.5f), input.texcoord.y);
    }
    else if (input.emitterType == 3)
    {
        float32_t2 distance = abs(input.texcoord - 0.5f);
        alpha = step(distance.x + distance.y, 0.5f);
    }
    return float32_t4(input.color.rgb * gDirectionalLight.color.rgb * light, input.color.a * alpha);
}
