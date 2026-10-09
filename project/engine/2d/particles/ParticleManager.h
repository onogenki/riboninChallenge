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

struct ParticlePlaybackSnapshot
{
    size_t count = 0;
    Transform transform{};
    Vector3 velocity{};
    float currentTime = 0.0f;
    float lifeTime = 0.0f;
    bool isEndless = false;
};

// 粒子グループの生成・更新・描画と、風や時間の逆再生をまとめて管理します。
class ParticleManager
{
public:

    // 最大インスタンス数 (1グループあたり)
    static const uint32_t kNumMaxInstance = 1000;

    // シングルトンインスタンスの取得
    static ParticleManager* GetInstance();

    // 初期化済みのDirectXとSRV管理を借り、粒子専用の描画資源を作ります。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    // Cameraの更新後に呼び、風・粒子・描画用データをこの順で1フレーム進めます。
    void Update();
    // Update済みの各グループだけを、現在のCamera用データで描画します。
    void Draw();
    // 途中で初期化に失敗した時も、所有するGroupと共通GPU資源を解放します。
    void Finalize();

	//パーティクルの発生
	// 名前で既存グループを探して粒子を追加します。名前は変更しないためコピーしません。
	void Emit(const std::string& name, const Vector3& position, uint32_t count,bool receivesWind, float scaleMultiplier = 1.0f);
	// あらかじめ用意した演出を発生する。実装はParticleManagerEffects.cppにまとめる。
	// 既存グループへ斬撃状の粒子を追加します。名前は検索専用です。
	void EmitHitEffect(const std::string& name, uint32_t count, const Vector3& translate, float scaleMultiplier = 1.0f);
	// 既存グループへ円状の粒子を追加します。名前は検索専用です。
    void EmitRingEffect(const std::string& name, uint32_t count, const Vector3& translate, float scaleMultiplier = 1.0f);
	// 既存グループへ円柱状の粒子を追加します。名前は検索専用です。
    void EmitCylinderEffect(const std::string& name, uint32_t count, const Vector3& translate, float scaleMultiplier = 1.0f);
	// 既存グループへ柱の火花を追加します。名前は検索専用です。
    void EmitPillarSparkle(const std::string& name, uint32_t count, const Vector3& position, float scaleMultiplier = 1.0f);
	// 既存グループへ光の中心粒子を追加します。名前は検索専用です。
    void EmitLightCore(const std::string& name, uint32_t count, const Vector3& position, float scaleMultiplier = 1.0f);
	// 既存グループへ光の雨粒子を追加します。名前は検索専用です。
    void EmitLightRain(const std::string& name, uint32_t count, const Vector3& position, float scaleMultiplier = 1.0f);
	// 既存グループへ渦巻き状の光を追加します。名前は検索専用です。
    void EmitLightSpiral(const std::string& name, uint32_t count, const Vector3& translate, float scaleMultiplier = 1.0f);

    // 名前とTextureで通常粒子用グループを作り、登録できた時だけtrueを返します。
    // 置換時の参照切れを避けるため、名前とTextureは値として受け取ります。
	bool CreateParticleGroup(const std::string name, const std::string textureFilePath);
    // 通常粒子と同じ管理下に円環頂点を作り、登録できた時だけtrueを返します。
    bool CreateRingParticleGroup(const std::string name, const std::string textureFilePath);
    // 分割数と寸法を使って円柱を作り、寸法・Texture・登録が有効な時だけtrueを返します。
    bool CreateCylinderParticleGroup(
        const std::string name,
        const std::string textureFilePath,
        uint32_t divide = 32,
        float topRadius = 1.0f,
        float bottomRadius = 1.0f,
        float height = 3.0f);

    // 全てのパーティクル（粒子）を削除する
    void ClearAllParticles();
    // 指定グループの粒子だけを消し、検索名とグループ自体は残します。
    void ClearParticles(const std::string& name);

    // 全グループのSRVを返却し、粒子・形状・再生履歴もまとめて消します。
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
	// 指定グループの先頭粒子をsnapshotへ写し、グループがなければfalseを返します。
	bool GetPlaybackSnapshot(const std::string& name, ParticlePlaybackSnapshot& snapshot) const;
	// 全グループの粒子数を返し、Debug表示や自動確認で使います。
	size_t GetTotalParticleCount() const;

    // グループがなければfalse、あればCamera向き描画の設定値を返します。
    bool GetBillboardEnabled(const std::string& name) const;
    // 存在するグループだけのCamera向き描画を切り替えます。
    void SetBillboardEnabled(const std::string& name, bool isEnabled);

	// 風の範囲に粒子が入っているかを読み取りだけで判定し、範囲のコピーを避けます。
	bool IsCollision(const MyMath::AABB& aabb, const Vector3& point);
private:
	// 粒子の状態・風・GPU転送形式はManagerの内部実装だけで使います。
	struct Particle
	{
		Transform transform{};
		Vector3 velocity{};
		Vector4 color{};
		float lifeTime = 0.0f;
		float currentTime = 0.0f;
		bool receivesWind = false; // 風を受けるかどうか
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
		Vector3 acceleration{}; // 加速度
		MyMath::AABB area{}; // 範囲
	};

	struct ParticleForGPU
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Vector4 color;
	};

	// マテリアル（定数バッファ用）
	struct Material
	{
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	// 頂点データ
	struct VertexData
	{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// 粒子一覧と描画資源をManagerだけが所有し、外部から直接変更させません。
	struct ParticleGroup
	{
		using UpdateCallback = void (*)(Particle& particle, float signedDeltaTime, bool returning);
		std::string textureFilePath;
		std::list<Particle> particles;
		uint32_t instancingSrvIndex = UINT32_MAX;
		Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
		uint32_t instanceCount = 0;
		ParticleForGPU* mappedData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		uint32_t vertexCount = 6;
		bool useBillboard = true;
		UpdateCallback updateCallback = nullptr;
	};

	// 描画で使うResourceの結合位置を、Pipeline生成より先に決めます。
	void InitializeRootSignature(ID3D12Device* device);
	// Shaderと描画状態を組み合わせ、粒子専用のPipelineを作ります。
	void InitializeGraphicsPipeline(ID3D12Device* device);
	// 通常粒子の板ポリゴンを作り、各グループの標準形状に使います。
	void InitializeDefaultQuad();
	// 粒子に共通する色とUV変換の初期値をGPUへ渡します。
	void InitializeMaterial();
	// 各形状に共通する領域を作り、失敗時は未完成のGroupを登録しません。
	bool InitializeInstancingResources(ParticleGroup& group);
	// 登録前にTextureと粒子用SRVの空きを確認し、読めない画像のグループを残しません。
	bool PrepareGroupResources(const std::string& textureFilePath);
	// 新しいGroupが完成した後だけ、同名の古いGroupを削除します。
	void RemoveExistingGroup(const std::string& name);
	static void UpdateCylinderParticle(Particle& particle, float signedDeltaTime, bool returning);
	// 風の切替時刻だけを進め、粒子の移動より先に当フレームの風状態を決めます。
	void UpdateWind(float signedDeltaTime, bool returning);
	// グループ固有の動き、渦巻き、風による速度変化を元の順番で反映します。
	void UpdateParticleMotion(Particle& particle, ParticleGroup& group, float signedDeltaTime, bool returning);
	// 寿命と色・大きさを進め、GPUへ渡す最終アルファ値を返します。
	float UpdateParticleAppearance(Particle& particle, float signedDeltaTime, bool returning);
	// 更新済みの粒子から行列を作り、上限内のインスタンスだけGPU用領域へ書き込みます。
	void WriteParticleGpuData(const Particle& particle, ParticleGroup& group,
		const Matrix4x4& viewProjectionMatrix, const Matrix4x4& billboardMatrix, float alpha);
	// 風の切替周期と渦巻きの最小半径は、逆再生でも同じ値を使います。
	static constexpr float kWindSwitchInterval = 5.0f;
	static constexpr float kMinimumSpiralRadius = 0.05f;
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

