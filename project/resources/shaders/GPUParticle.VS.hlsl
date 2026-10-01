struct Particle
{
    float32_t3 translate;
    float32_t3 scale;
    float lifeTime;
    float32_t3 velocity;
    float currentTime;
    float32_t4 color;
    uint32_t particleType;
    uint32_t emitterType;
};

struct PerView
{
    float32_t4x4 viewProjectionMatrix;
    float32_t4x4 billboardMatrix;
};

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
};

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
    uint32_t emitterType : EMITTERTYPE0;
};

StructuredBuffer<Particle> gParticles : register(t0);
StructuredBuffer<uint32_t> gActiveParticleIndices : register(t1);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID)
{
    uint32_t particleIndex = gActiveParticleIndices[instanceId];
    Particle particle = gParticles[particleIndex];
    float32_t4x4 worldMatrix = gPerView.billboardMatrix;
    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    worldMatrix[3].xyz = particle.translate;
    if (particle.particleType == 1)
    {
        worldMatrix[0] *= 1.0f + length(particle.velocity) * 8.0f;
    }

    VertexShaderOutput output;
    output.position = mul(mul(input.position, worldMatrix), gPerView.viewProjectionMatrix);
    output.texcoord = input.texcoord;
    output.color = particle.color;
    output.emitterType = particle.emitterType;
    return output;
}
