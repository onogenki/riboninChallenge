#include"Particle.hlsli"

// パーティクルのテクスチャ色と頂点色を合成するピクセルシェーダー。
struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
};

struct DirectionalLight
{
    float32_t4 color; //ライトの色
    float32_t3 direction; //ライトの向き
    float intensity; //輝度
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};
ConstantBuffer<Material> gMaterial : register(b0);

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float32_t4 uv = mul(float32_t4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float32_t4 tex = gTexture.Sample(gSampler, uv.xy);

	// ライティング有効時だけ拡散反射を計算し、無効時は頂点色をそのまま利用する。
	if (gMaterial.enableLighting != 0)
    {
        float32_t3 normal = normalize(input.normal);
        float32_t3 lightDir = normalize(-gDirectionalLight.direction);
        float NdotL = saturate(dot(normal, lightDir));
        float32_t3 diffuse = gMaterial.color.rgb * tex.rgb * input.color.rgb * gDirectionalLight.color.rgb * NdotL * gDirectionalLight.intensity;
        output.color.rgb = diffuse;
        output.color.a = gMaterial.color.a * tex.a * input.color.a;

    }
    else
    {
        output.color = gMaterial.color * tex * input.color;
    }
    
	// 透明なテクセルを破棄して、板ポリゴンの輪郭を見せない。
	if (tex.a <= 0.0f)
        discard;
    return output;
}
