#include "Mirror.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

MirrorVertexShaderOutput main(VertexShaderInput input)
{
    MirrorVertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;
    return output;
}
