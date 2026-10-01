#pragma once
#include <string>
#include <unordered_map>
#include <list>
#include <random>
#include <cassert>
#include <d3d12.h>
#include <wrl.h>
#include "Vector3.h"
#include "MyMath.h"
#include "Object3d.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "TimePlayback.h"

class CameraManager;

struct Particle{
public:
    Transform transform{};
    Vector3 velocity{};
    Vector4 color{};
    float lifeTime = 0.0f;
    float currentTime = 0.0f;
    bool receivesWind = false;//風を受けるかどうか
    bool isEndless = false;
    float scaleVelocityY = 0.0f;

    bool isSpiral = false;
    Vector3 spiralCenter{};
    Vector3 spiralRight{ 1.0f, 0.0f, 0.0f };
    Vector3 spiralUp{ 0.0f, 1.0f, 0.0f };
    float spiralAngle = 0.0f;
    float spiralRadius = 0.0f;
    float spiralAngularVelocity = 0.0f;
    float spiralRadialVelocity = 0.0f;

    bool useColorAndScaleOverLife = false;
    Vector3 startScale{};
    Vector3 endScale{};
    Vector4 startColor{};
    Vector4 endColor{};
};

struct AccelerationField
{
    Vector3 acceleration{};//加速度
    MyMath::AABB area{};//範囲
};

struct ParticleForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
    Vector4 color;
};

// マテリアル（定数バッファ用）
struct Material {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
	Matrix4x4 uvTransform;
};

// 頂点データ
struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

struct ParticleGroup
{
	using UpdateCallback = void (*)(Particle& particle, float signedDeltaTime, bool returning);
    // マテリアルデータ
    std::string textureFilePath;
	uint32_t textureSrvIndex = UINT32_MAX;
    // パーティクルのリスト
    std::list<Particle> particles;
    // インスタンシング用データ
	uint32_t instancingSrvIndex = UINT32_MAX;
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
	uint32_t instanceCount = 0;
    // インスタンシングデータを書き込むためのポインタ
	ParticleForGPU* mappedData = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
    uint32_t vertexCount = 6;
    bool useBillboard = true;
	UpdateCallback updateCallback = nullptr;
};

struct ParticlePlaybackSnapshot
{
    size_t count = 0;
    Transform transform{};
    Vector3 velocity{};
    float currentTime = 0.0f;
    float lifeTime = 0.0f;
    bool isEndless = false;
};

class ParticleManager
{
public:

    // 最大インスタンス数 (1グループあたり)
    static const uint32_t kNumMaxInstance = 1000;

    // シングルトンインスタンスの取得
    static ParticleManager* GetInstance();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Update();
    void Draw();
    void Finalize() {
        // インスタンス自体を消すのではなく、中身（グループリストなど）を掃除する
        ClearAllGroups();
    }

	//パーティクルの発生
	void Emit(const std::string name, const Vector3& position, uint32_t count,bool receivesWind, float scaleMultiplier = 1.0f);
	// あらかじめ用意した演出を発生する。実装はParticleManagerEffects.cppにまとめる。
	void EmitHitEffect(const std::string name, uint32_t count, const Vector3& translate, float scaleMultiplier = 1.0f);
    void EmitRingEffect(const std::string name, uint32_t count, const Vector3& translate, float scaleMultiplier = 1.0f);
    void EmitCylinderEffect(const std::string name, uint32_t count, const Vector3& translate, float scaleMultiplier = 1.0f);
    void EmitPillarSparkle(const std::string name, uint32_t count, const Vector3& position, float scaleMultiplier = 1.0f);
    void EmitLightCore(const std::string name, uint32_t count, const Vector3& position, float scaleMultiplier = 1.0f);
    void EmitLightRain(const std::string name, uint32_t count, const Vector3& position, float scaleMultiplier = 1.0f);
    void EmitLightSpiral(const std::string name, uint32_t count, const Vector3& translate, float scaleMultiplier = 1.0f);

	void CreateParticleGroup(const std::string name, const std::string textureFilePath);
    void CreateRingParticleGroup(const std::string name, const std::string textureFilePath);
    void CreateCylinderParticleGroup(
        const std::string name,
        const std::string textureFilePath,
        uint32_t divide = 32,
        float topRadius = 1.0f,
        float bottomRadius = 1.0f,
        float height = 3.0f);

    // 全てのパーティクル（粒子）を削除する
    void ClearAllParticles();
    void ClearParticles(const std::string name);

    // 特定のグループのパーティクルだけを削除する
    void ClearAllGroups();

    void SetCameraManager(CameraManager* cameraManager) { cameraManager_ = cameraManager; }
    bool IsWindEnabled() const { return isField_; }
    void SetWindEnabled(bool isEnabled) { isField_ = isEnabled; windTimer_ = 0.0f; }
    void ToggleWind() { SetWindEnabled(!isField_); }
    bool IsAutoWindSwitchEnabled() const { return autoWindSwitch_; }
    void SetAutoWindSwitchEnabled(bool isEnabled) { autoWindSwitch_ = isEnabled; windTimer_ = 0.0f; }
    Vector3 GetWindAcceleration() const { return accelerationField_.acceleration; }
    void SetWindAcceleration(const Vector3& acceleration) { accelerationField_.acceleration = acceleration; }
	bool IsReturning() const { return returnState_.IsReturning(); }
	void SetReturning(bool returning) { returnState_.SetReturning(returning); }
	bool GetPlaybackSnapshot(const std::string& name, ParticlePlaybackSnapshot& snapshot) const;
	size_t GetTotalParticleCount() const;

    bool GetBillboardEnabled(const std::string& name) const;
    void SetBillboardEnabled(const std::string& name, bool isEnabled);

	bool IsCollision(const MyMath::AABB aabb, const Vector3& point);
private:
	static void UpdateCylinderParticle(Particle& particle, float signedDeltaTime, bool returning);
    ParticleManager() = default;
    ~ParticleManager() = default;
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;


    // メンバ変数に記録するポインタ
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    CameraManager* cameraManager_ = nullptr;

    //グループを複数持てるように
    std::unordered_map<std::string, ParticleGroup> particleGroups_;
    // ランダムエンジン
    std::mt19937 randomEngine_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    //場
	AccelerationField accelerationField_{};
	ReturnPlaybackState returnState_;
    bool autoWindSwitch_ = true;
    float windTimer_ = 0.0f;//吹く時間
    bool isField_ = false;//風が吹いてるかどうか
};

