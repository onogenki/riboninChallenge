#include"Object3d.hlsli"

// 3Dモデルの材質、3種類のライト、環境マップを合成するピクセルシェーダー。
struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float shininess;
    float environmentCoefficient;
    float specularIntensity;
    float padding;
};

struct DirectionalLight
{
    float32_t4 color; //ライトの色
    float32_t3 direction; //ライトの向き
    float intensity; //輝度

    // 直接光が当たらない壁や天井を、最低限見える明るさへ保つ環境光です。
    float32_t3 ambientColor;
    float ambientIntensity;
};

struct PointLight
{
    float32_t4 color; //ライトの色
    float32_t3 position; //ライトの位置
    float intensity; //輝度
    float radius; //ライトの届く最大距離
    float decay; //減衰率
};

struct SpotLight
{
    float32_t4 color; //ライトの色
    float32_t3 position; //ライトの位置
    float intensity; //輝度
    float32_t3 direction; //スポットライトの方向
    float distance; //ライトの届く最大距離
    float decay; //減衰率
    float cosAngle; //スポットライトの余韻
    float cosFalloffStart; //Falloff開始の角度
};

struct SpotLightSet
{
    SpotLight lights[16];
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

struct Camera
{
    float32_t3 worldPosition;
    float32_t backFaceBrightness;
    float32_t opacity;
    float32_t specularBoost;
    float32_t glowIntensity;
    float32_t padding;
    float32_t3 warningDirection;
    float32_t warningStrength;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLightSet> gSpotLights : register(b4);

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);
TextureCube<float32_t4> gEnvironmentTexture : register(t1);

PixelShaderOutput main(
    VertexShaderOutput input,
    bool isFrontFace : SV_IsFrontFace)
{
	// ここでのinputは頂点シェーダーによりワールド座標へ変換済み。
    PixelShaderOutput output;
    
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    // 環境光は法線の向きに関係なく足し、室内の影側を真っ黒にしません。
    float32_t3 ambientLight =
        gMaterial.color.rgb * textureColor.rgb *
        gDirectionalLight.ambientColor * gDirectionalLight.ambientIntensity;
    
    // 厚みのない板Modelは裏側からも描画するため、裏面では法線も反転してLightを計算する。
    // CullModeをNoneにするだけでは法線は反転しないため、Mirror・看板などが暗く消えるのを防ぐ。
    float32_t3 N = normalize(input.normal);
    if (!isFrontFace)
    {
        N = -N;
    }
    //視線ベクトル
    float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    // 透過中の球は奥側の面を重ねない。表面越しに球の内側が見える違和感を防ぎます。
    if (gCamera.opacity < 1.0f && dot(input.normal, toEye) < 0.0f)
    {
        discard;
    }
    
    ///
    ///平行光源
    ///
    // directionは面から光源へ向かう向きです。法線との内積で、面に届く割合を求めます。
    
    float32_t3 L = gDirectionalLight.direction;
    float cosDirectional = saturate(dot(N, L));
    
    //拡散反射
    float32_t3 diffuseDirectionalLight =
        gMaterial.color.rgb *
        textureColor.rgb *
        gDirectionalLight.color.rgb *
        cosDirectional *
        gDirectionalLight.intensity;

    // 鏡面反射
    float32_t3 reflectedDirectional = reflect(-L, N);
    float NDotHDirectional = dot(toEye, reflectedDirectional);
    float specularPowDirectional =
        pow(saturate(NDotHDirectional), gMaterial.shininess);
    float32_t3 specularDirectionalLight =
        gDirectionalLight.color.rgb *
        gDirectionalLight.intensity *
        specularPowDirectional *
        gMaterial.specularIntensity *
        float32_t3(1.0f, 1.0f, 1.0f);
    
    ///
    ///点光源の計算
    ///
    
    // 点光源は位置を持つため、光源からピクセルへの向きと距離を毎回求めます。
    
    float32_t3 pointLightDirection = normalize(input.worldPosition - gPointLight.position);
    
    //減衰係数の計算
    float pointDistance = length(gPointLight.position - input.worldPosition);
    float pointFactor = pow(saturate(-pointDistance / gPointLight.radius + 1.0f), gPointLight.decay); 
    
    //拡散反射
    float cosPoint = saturate(dot(N, -pointLightDirection));
    float32_t3 diffusePointLight =
    gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * cosPoint * gPointLight.intensity * pointFactor;
    //鏡面反射
    float32_t3 reflectedPoint = reflect(pointLightDirection, N);
    float NDotHPoint = dot(toEye, reflectedPoint);
    float specularPowPoint = pow(saturate(NDotHPoint), gMaterial.shininess);
    float32_t3 specularPointLight =
    gPointLight.color.rgb * gPointLight.intensity * pointFactor * specularPowPoint *
        gMaterial.specularIntensity * float32_t3(1.0f, 1.0f, 1.0f);
    
    ///
    ///スポットライト
    ///
    
    // 複数本のLightを加算し、Laserの周囲が実際に明るく見えるようにします。
    float32_t3 diffuseSpotLight = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t3 specularSpotLight = float32_t3(0.0f, 0.0f, 0.0f);
    [unroll]
    for (int spotIndex = 0; spotIndex < 16; ++spotIndex)
    {
        SpotLight spotLight = gSpotLights.lights[spotIndex];
        if (spotLight.intensity <= 0.0f || spotLight.distance <= 0.0f)
        {
            continue;
        }

        // ライトからピクセルへの方向、距離、円錐の内側にいる割合を求めます。
        // intensityかdistanceが0の枠は未使用として飛ばし、16枠の配列を毎回安全に走査します。
        float32_t3 spotLightDirectionOnSurface = normalize(input.worldPosition - spotLight.position);
        float distanceSpot = length(spotLight.position - input.worldPosition);
        float attenuationFactor = pow(saturate(-distanceSpot / spotLight.distance + 1.0f), spotLight.decay);
        float cosAngle = dot(spotLightDirectionOnSurface, spotLight.direction);
        float falloffFactor = saturate((cosAngle - spotLight.cosAngle) / max(spotLight.cosFalloffStart - spotLight.cosAngle, 0.00001f));

        float cosSpot = saturate(dot(N, -spotLightDirectionOnSurface));
        diffuseSpotLight +=
            gMaterial.color.rgb * textureColor.rgb * spotLight.color.rgb *
            cosSpot * spotLight.intensity * attenuationFactor * falloffFactor;

        float32_t3 reflectedSpot = reflect(spotLightDirectionOnSurface, N);
        float specularPowSpot = pow(saturate(dot(toEye, reflectedSpot)), gMaterial.shininess);
        specularSpotLight +=
            spotLight.color.rgb * spotLight.intensity * attenuationFactor * falloffFactor *
            specularPowSpot * gMaterial.specularIntensity * float32_t3(1.0f, 1.0f, 1.0f);
    }
    
    // 世界空間の法線とLight方向で球の片側だけに反射を出し、Playerの回転から独立させます。
    float32_t warningLevel = saturate(gCamera.warningStrength);
    float32_t warningThreshold = lerp(0.94f, -1.10f, warningLevel);
    float32_t warningMask = warningLevel * smoothstep(
        warningThreshold - 0.10f,
        warningThreshold + 0.10f,
        dot(N, normalize(gCamera.warningDirection)));
    // 反射範囲が広がるにつれ、青白さから黄色へ滑らかに変えます。
    float32_t3 warningColor = lerp(
        float32_t3(0.55f, 0.80f, 1.0f),
        float32_t3(1.0f, 0.86f, 0.15f),
        smoothstep(0.25f, 0.95f, warningLevel));

    //全ての光を合成
    output.color.rgb = ambientLight + diffuseDirectionalLight + diffusePointLight + diffuseSpotLight;
    // Player消失中だけ反射光を強め、ほかのObjectは倍率1で従来の見た目を保ちます。
    float32_t3 specularLight =
        specularDirectionalLight + specularPointLight + specularSpotLight;
    output.color.rgb +=
        specularLight * gCamera.specularBoost;
    output.color.rgb += specularLight * warningMask * 6.0f * warningColor;
    // ハイライトが届きにくい角度でも、光源側の小さな反射だけは見えるようにします。
    output.color.rgb += warningColor * warningMask * (0.15f + 0.70f * warningLevel);
    
    
	// 係数が0より大きいときだけキューブマップの反射光を加算する。
	if (gMaterial.environmentCoefficient > 0.0f)
    {
        float32_t3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
        float32_t3 reflectedVector = reflect(cameraToPosition, N);
        float32_t4 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector);
    
        output.color.rgb += environmentColor.rgb * gMaterial.environmentCoefficient;
    } 
    // アルファ
    output.color.a =
        gMaterial.color.a * textureColor.a * gCamera.opacity;
    
	// 完全透明なピクセルは深度・色バッファへ書き込まない。
	if (output.color.a == 0.0f)
    {
        discard;
    }
    // 反射が当たりにくい角度でも、輪郭寄りの青白い光で消失を見せます。
    float32_t rim = pow(1.0f - saturate(dot(N, toEye)), 2.0f);
    output.color.rgb += warningColor *
        gCamera.glowIntensity * (0.35f + 0.65f * rim);
    
   
    // 元の法線側を表面とします。携帯鏡の+Z法線と同じため、縦持ち・横持ちの両方で判別できます。
    // 通常Objectは倍率1なので、既存の材質・明るさを維持します。
    if (dot(input.normal, toEye) < 0.0f)
    {
        output.color.rgb *= gCamera.backFaceBrightness;
    }
    return output;
}
