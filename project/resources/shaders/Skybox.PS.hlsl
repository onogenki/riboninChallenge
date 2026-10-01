#include"Skybox.hlsli"

struct Material
{
    float32_t4 color;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

//Cubemapを利用するのに使う
TextureCube<float32_t4> gTexture : register(t0);

ConstantBuffer<Material> gMaterial : register(b0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    output.color = textureColor * gMaterial.color;
    return output;
}