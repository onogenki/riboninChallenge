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
    
    ///
    ///平行光源
    ///
    // ライト方向
    
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
    
    //入射光の方向
    
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

        // ライトから頂点への方向、距離、円錐の内側にいる割合を求めます。
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
    
    //全ての光を合成
    output.color.rgb = ambientLight + diffuseDirectionalLight + specularDirectionalLight + diffusePointLight + specularPointLight + diffuseSpotLight + specularSpotLight;
    
    
	// 係数が0より大きいときだけキューブマップの反射光を加算する。
	if (gMaterial.environmentCoefficient > 0.0f)
    {
        float32_t3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
        float32_t3 reflectedVector = reflect(cameraToPosition, N);
        float32_t4 enviromentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector);
    
        output.color.rgb += enviromentColor.rgb * gMaterial.environmentCoefficient;
    } 
    // アルファ
    output.color.a =
        gMaterial.color.a * textureColor.a;
    
	// 完全透明なピクセルは深度・色バッファへ書き込まない。
	if (output.color.a == 0.0f)
    {
        discard;
    }
    
   
    // 元の法線側を表面とします。携帯鏡の+Z法線と同じため、縦持ち・横持ちの両方で判別できます。
    // 通常Objectは倍率1なので、既存の材質・明るさを維持します。
    if (dot(input.normal, toEye) < 0.0f)
    {
        output.color.rgb *= gCamera.backFaceBrightness;
    }
    return output;
}
