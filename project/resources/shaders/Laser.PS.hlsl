struct LaserData
{
    float32_t4x4 viewProjection;
    float32_t4 color;
};

ConstantBuffer<LaserData> gLaser : register(b0);

struct PixelShaderInput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};

float32_t4 main(PixelShaderInput input) : SV_TARGET0
{
    // 0が帯の中心、1が縁です。縁を徐々に透明にして光の体積感を作ります。
    float edgeDistance = abs(input.texcoord.x * 2.0f - 1.0f);
    float outerGlow = 1.0f - smoothstep(0.20f, 1.0f, edgeDistance);
    float coreGlow = 1.0f - smoothstep(0.0f, 0.24f, edgeDistance);

    // 線分の端を少し弱め、直方体ではなく発光している帯に見せます。
    float endFade = smoothstep(0.0f, 0.08f, input.texcoord.y) *
        (1.0f - smoothstep(0.92f, 1.0f, input.texcoord.y));
    float brightness = (outerGlow * 0.35f + coreGlow * 1.20f) * endFade;
    float alpha = (outerGlow * 0.22f + coreGlow * 0.68f) * endFade * gLaser.color.a;

    // 加算合成へ半透明の色を渡し、背景の形を残したまま明るい中心線を重ねます。
    return float32_t4(gLaser.color.rgb * brightness, saturate(alpha));
}
