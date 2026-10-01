#include "Object3d.hlsli"

// 通常のTextureやLightを使わず、壁越しに見せる形だけを一色で描きます。
struct OccludedSilhouetteData
{
    float32_t4 color;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

ConstantBuffer<OccludedSilhouetteData> gOccludedSilhouette : register(b5);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gOccludedSilhouette.color;
    return output;
}
