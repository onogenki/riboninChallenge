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

struct PerFrame
{
    float time;
    float deltaTime;
};

struct Field
{
    float32_t3 acceleration;
    float drag;
};

static const uint32_t kMaxParticles = 1024;
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<int32_t> gFreeList : register(u2);
RWStructuredBuffer<uint32_t> gActiveParticleIndices : register(u3);
RWStructuredBuffer<uint32_t> gDrawArguments : register(u4);
ConstantBuffer<PerFrame> gPerFrame : register(b1);
ConstantBuffer<Field> gField : register(b2);

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const uint32_t particleIndex = DTid.x;
    if (particleIndex == 0)
    {
        gDrawArguments[0] = 6;
        gDrawArguments[1] = 0;
        gDrawArguments[2] = 0;
        gDrawArguments[3] = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    if (particleIndex < kMaxParticles && gParticles[particleIndex].color.a != 0.0f)
    {
        Particle particle = gParticles[particleIndex];
        particle.velocity += gField.acceleration * gPerFrame.deltaTime;
        particle.velocity *= saturate(1.0f - gField.drag * gPerFrame.deltaTime);
        particle.translate += particle.velocity * gPerFrame.deltaTime;
        particle.currentTime += gPerFrame.deltaTime;
        float alpha = 1.0f - particle.currentTime / particle.lifeTime;
        particle.color.a = saturate(alpha);
        if (particle.color.a == 0.0f)
        {
            particle.scale = float32_t3(0.0f, 0.0f, 0.0f);
            int32_t freeListIndex;
            InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
            if (freeListIndex < kMaxParticles - 1)
            {
                gFreeList[freeListIndex + 1] = particleIndex;
            }
            else
            {
                InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
            }
        }
        gParticles[particleIndex] = particle;
        if (particle.color.a != 0.0f)
        {
            uint32_t activeParticleIndex;
            InterlockedAdd(gDrawArguments[1], 1, activeParticleIndex);
            gActiveParticleIndices[activeParticleIndex] = particleIndex;
        }
    }
}
