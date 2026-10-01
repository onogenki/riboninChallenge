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

struct Emitter
{
    float32_t3 translate;
    float radius;
    float32_t3 boxSize;
    uint32_t count;
    float frequency;
    float frequencyTime;
    uint32_t emit;
    uint32_t type;
    uint32_t particleType;
    uint32_t3 padding;
};

struct PerFrame
{
    float time;
    float deltaTime;
};

float rand3dTo1d(float32_t3 value, float32_t3 dotDirection)
{
    float smallValue = sin(dot(value, dotDirection));
    return frac(smallValue * 143758.5453f);
}

float32_t3 rand3dTo3d(float32_t3 value)
{
    return float32_t3(
        rand3dTo1d(value, float32_t3(12.9898f, 78.233f, 37.719f)),
        rand3dTo1d(value, float32_t3(39.3468f, 11.135f, 83.155f)),
        rand3dTo1d(value, float32_t3(73.156f, 52.235f, 9.151f)));
}

class RandomGenerator
{
    float32_t3 seed;

    float32_t3 Generate3d()
    {
        seed = rand3dTo3d(seed);
        return seed;
    }

    float Generate1d()
    {
        seed = rand3dTo3d(seed + float32_t3(41.731f, 17.903f, 91.147f));
        return seed.x;
    }
};

static const uint32_t kMaxParticles = 1024;
static const float32_t3 kMeshTriangles[12] = {
    float32_t3(0.0f, 0.15f, 0.0f), float32_t3(-0.15f, -0.15f, 0.15f), float32_t3(0.15f, -0.15f, 0.15f),
    float32_t3(0.0f, 0.15f, 0.0f), float32_t3(0.15f, -0.15f, 0.15f), float32_t3(0.0f, -0.15f, -0.15f),
    float32_t3(0.0f, 0.15f, 0.0f), float32_t3(0.0f, -0.15f, -0.15f), float32_t3(-0.15f, -0.15f, 0.15f),
    float32_t3(-0.15f, -0.15f, 0.15f), float32_t3(0.0f, -0.15f, -0.15f), float32_t3(0.15f, -0.15f, 0.15f),
};
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int32_t> gFreeListIndex : register(u1);
RWStructuredBuffer<int32_t> gFreeList : register(u2);
ConstantBuffer<Emitter> gEmitter : register(b0);
ConstantBuffer<PerFrame> gPerFrame : register(b1);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit != 0 && DTid.x < gEmitter.count)
    {
        RandomGenerator generator;
        generator.seed = (float32_t3(DTid) + gPerFrame.time) * gPerFrame.time;
        int32_t freeListIndex;
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
        if (freeListIndex >= 0)
        {
            uint32_t particleIndex = gFreeList[freeListIndex];
            Particle particle = (Particle)0;
            particle.scale = 0.05f + generator.Generate3d() * 0.05f;
            particle.lifeTime = 0.5f;
            float32_t3 direction = generator.Generate3d() * 2.0f - 1.0f;
            float directionLengthSquared = dot(direction, direction);
            if (directionLengthSquared < 0.0001f)
            {
                direction = float32_t3(0.0f, 1.0f, 0.0f);
            }
            else
            {
                direction *= rsqrt(directionLengthSquared);
            }
            uint32_t emitterType = gEmitter.type;
            if (emitterType == 4)
            {
                emitterType = min(uint32_t(generator.Generate1d() * 4.0f), 3);
            }
            if (emitterType == 0)
            {
                float radius = pow(generator.Generate1d(), 1.0f / 3.0f) * gEmitter.radius;
                particle.translate = gEmitter.translate + direction * radius;
                particle.color.rgb = float32_t3(0.4f, 0.8f, 1.0f);
            }
            else if (emitterType == 1)
            {
                particle.translate = gEmitter.translate + (generator.Generate3d() * 2.0f - 1.0f) * gEmitter.boxSize;
                particle.color.rgb = float32_t3(0.4f, 1.0f, 0.5f);
            }
            else if (emitterType == 2)
            {
                float angle = generator.Generate1d() * 6.28318530718f;
                float height = generator.Generate1d() * gEmitter.boxSize.y;
                float radius = (1.0f - height / gEmitter.boxSize.y) * gEmitter.radius;
                particle.translate = gEmitter.translate + float32_t3(cos(angle) * radius, height, sin(angle) * radius);
                particle.color.rgb = float32_t3(1.0f, 0.7f, 0.2f);
            }
            else
            {
                uint32_t triangleIndex = min(uint32_t(generator.Generate1d() * 4.0f), 3) * 3;
                float u = generator.Generate1d();
                float v = generator.Generate1d();
                if (u + v > 1.0f)
                {
                    u = 1.0f - u;
                    v = 1.0f - v;
                }
                float32_t3 a = kMeshTriangles[triangleIndex];
                float32_t3 b = kMeshTriangles[triangleIndex + 1];
                float32_t3 c = kMeshTriangles[triangleIndex + 2];
                particle.translate = gEmitter.translate + a + (b - a) * u + (c - a) * v;
                particle.color.rgb = float32_t3(1.0f, 0.3f, 0.8f);
            }
            particle.velocity = (generator.Generate3d() - 0.5f) * 0.3f;
            particle.color.a = 1.0f;
            particle.particleType = gEmitter.particleType;
            particle.emitterType = emitterType;
            gParticles[particleIndex] = particle;
        }
        else
        {
            InterlockedAdd(gFreeListIndex[0], 1, freeListIndex);
        }
    }
}
