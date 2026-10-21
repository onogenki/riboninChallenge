#pragma once
#include "MyMath.h"
#include "Transform.h"
#include "Model.h"
#include "TimePlayback.h"
#include <vector>
#include <string>
#include <array>
#include <memory>

class Object3dCommon;
class Object3dGpuData;
class Object3dFactory;
class Camera;

// 一体分のTransform・Animation・Light描画データを所有し、頂点と材質は共有Modelから借ります。
class Object3d
{
public:
	// 一つのSceneで同時に使用できる、実際に面を照らすSpotLightの本数です。
	static constexpr size_t kMaximumSpotLightCount = 16;

	//座標変換行列データ
	struct TransformationMatrix
	{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};

	// 平行光源データ
	struct DirectionalLight {
		Vector4 color{};
		// 物体の面から光源を見る方向です。上から照らす場合はYを正にします。
		Vector3 direction{};
		float intensity = 0.0f;//明るさの強さ
		// 光が直接届かない面も完全な黒にしない、間接光風の最低限の明るさです。
		Vector3 ambientColor{};
		float ambientIntensity = 0.0f;
	};

	struct PointLight
	{
		Vector4 color{};//ライトの色
		Vector3 position{};//ライトの位置
		float intensity = 0.0f;//輝度
		float radius = 0.0f;//ライトの届く最大距離
		float decay = 0.0f;//減衰率
		float padding[2]{};
	};

	struct SpotLight
	{
		Vector4 color{};//ライトの色
		Vector3 position{};//ライトの位置
		float intensity = 0.0f;//輝度
		Vector3 direction{};//スポットライトの方向
		float distance = 0.0f;//ライトの届く最大距離
		float decay = 0.0f;//減衰率
		float cosAngle = 0.0f;//スポットライトの余弦
		float cosFalloffStart = 0.0f;
		float padding[1]{};
	};

	// HLSLのSpotLight配列と同じ並びでConstant Bufferへ渡すデータです。
	struct SpotLightSet
	{
		std::array<SpotLight, kMaximumSpotLightCount> lights{};
	};
	// HLSLのSpotLight配列は一要素64byteなので、CPU側の並びも同じ大きさを保証します。
	static_assert(sizeof(SpotLight) == 64);

	struct CameraForGPU
	{
		Vector3 worldPosition;
		// 通常は1。板の裏面を区別したいObjectだけ、1より小さくします。
		float backFaceBrightness = 1.0f;
	};
	// HLSLのfloat3 + floatと同じ16byte配置を維持します。
	static_assert(sizeof(CameraForGPU) == 16);

	struct ReflectionData
	{
		Matrix4x4 reflectionViewProjection;
		Vector4 tint;
	};

	// 壁に隠れた時だけ表示する、半透明シルエットの色です。
	struct OccludedSilhouetteData
	{
		Vector4 color;
	};

	Object3d();
	~Object3d();

	void Update();
	// Animation時間を進めず、指定Camera用の行列だけを描画直前に更新する
	void UpdateCameraForDraw(Camera* drawCamera);

	void Draw();
	// 通常描画済みの壁より奥にある部分だけを、指定色の半透明シルエットとして重ねます。
	// 通常描画では自動実行しません。壁越しに見せたいObjectだけ、Scene側で明示的に呼びます。
	void DrawOccludedSilhouette(const Vector4& color);
	// 反射CameraのTextureを鏡面へ投影して描画する
	void DrawMirror(uint32_t reflectionTextureSrvIndex, const Matrix4x4& reflectionViewProjection);

	bool GetJointWorldMatrix(const std::string& jointName, Matrix4x4& worldMatrix) const;
	void SetParentWorldMatrix(const Matrix4x4& parentWorldMatrix);
	void ClearParentWorldMatrix();

	void PlayAnimation(const Model::Animation& animation);
	void SetAnimationPlaying(bool isPlaying) { isAnimating_ = isPlaying; }

	//ループするかどうかの関数(falseで1ループのみ)
	void SetIsLoop(bool isLoop) { isLoop_ = isLoop; }

	//最大で何秒まで再生するか設定
	void SetMaxPlayTime(float maxTime) { maxPlayTime_ = maxTime; }

	//今何秒か教える関数
	float GetAnimationTime() const { return animationTime_; }
	//シーンからアニメーションの時間をいじる関数
	void SetAnimationTime(float time) { this->animationTime_ = time; }
	bool IsAnimating() const { return isAnimating_; }
	float GetAnimationDuration() const { return currentAnimation_.duration; }
	bool IsAnimationReturning() const { return animationReturnState_.IsReturning(); }
	void SetAnimationReturning(bool returning) { animationReturnState_.SetReturning(returning && isAnimating_); }

	void RecordTransformEdit(const Transform& before);
	void RecordTransformEdit(const Transform& before, float elapsedSeconds);
	bool IsTransformReturning() const { return transformPlayback_.IsReturning(); }
	void SetTransformReturning(bool returning) { transformPlayback_.SetReturning(returning); }
	bool CanMoveTransformForward() const { return transformPlayback_.CanMoveForward(); }
	bool MoveTransformForward() { return transformPlayback_.StartMoveForward(); }
	bool IsTransformMovingForward() const { return transformPlayback_.IsMovingForward(); }
	bool HasTransformHistory() const { return transformPlayback_.HasHistory(); }
	float GetTransformPlaybackProgress() const { return transformPlayback_.GetProgress(); }
	float GetTransformPlaybackTime() const { return transformPlayback_.GetPlaybackTime(); }
	float GetTransformPlaybackDuration() const { return transformPlayback_.GetDuration(); }

	// 描画用のSkeletonを読み取り専用で返し、外部からJointの状態を変更させません。
	const Model::Skeleton& GetSkeleton() const { return skeleton_; }

	// 位置・回転・拡大率をObject自身へ保存し、次のUpdateで描画行列へ反映します。
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	// 編集済みの三要素をまとめて反映し、内部Transformの変更経路をObject3dに限定します。
	void SetTransform(const Transform& newTransform) { transform_ = newTransform; }
	// 描画時に借りるCameraだけを切り替え、Camera自体の所有権は移しません。
	void SetCamera(Camera* camera) { camera_ = camera; }

	//構造体(color,direction,intensity)全部入ってるsetter
	void SetDirectionalLight(const DirectionalLight& light);
	void SetPointLight(const PointLight& light);
	void SetSpotLight(const SpotLight& light);
	// 複数のLightを同時に設定し、暗い部屋でもそれぞれのLightが周囲を照らせるようにします。
	void SetSpotLights(const std::array<SpotLight, kMaximumSpotLightCount>& lights);
	// 共有Modelの材質を変更します。同じModelを使う別Objectにも影響するため、個体別設定ではありません。
	void SetEnvironmentCoefficient(float coefficient);
	// 共有Modelの材質を変えず、このObjectの裏面だけを暗くします。
	void SetBackFaceBrightness(float brightness);
	// 読込成功時だけこのObjectのTextureを上書きし、失敗時は以前の設定を保ってfalseを返します。
	bool SetTextureOverride(const std::string& texturePath);
	void ClearTextureOverride() { textureSrvIndexOverride_ = UINT32_MAX; }

	// 描画と当たり判定が同じ値を読めるよう、Transformの各要素を読み取り専用で返します。
	const Vector3& GetScale()const { return transform_.scale; }
	const Vector3& GetRotate()const { return transform_.rotate; }
	const Vector3& GetTranslate()const { return transform_.translate; }
	const DirectionalLight& GetDirectionalLight() const;
	// このObjectが借りている共有Modelの環境反射率を読みます。
	float GetEnvironmentCoefficient() const;
	// 編集できるObjectからは、従来どおり共有Modelへの参照を貸します。
	Model* GetModel() { return model_; }
	// 読み取り専用Objectからは共有Modelの材質や頂点を変更させません。
	const Model* GetModel() const { return model_; }
	const std::string& GetModelName() const { return modelName_; }

	//外部からアニメーションモデルかどうか判定
	bool IsSkeletal() const { return isSkeletal_; }

	// 描画・当たり判定・編集履歴が同じ値を参照できるよう、Transformを読み取り専用で返します。
	const Transform& GetTransform() const { return transform_; }

private:
	// Object3dを描画可能な状態へ準備する手順は、Factoryだけが実行します。
	// Sceneやゲーム物体はFactoryから受け取った後に、位置・Light・Animation再生だけを設定します。
	friend class Object3dFactory;
	// 一体分のGPU描画データが完成した時だけFactoryへ成功を返します。
	bool Initialize(Object3dCommon* object3dCommon);
	void SetModel(Model* model) { this->model_ = model; }
	void SetModel(const std::string& filePath);
	bool InitializeAnimation();

	void ReleaseSkinClusterDescriptors();

	Object3dCommon* object3dCommon_ = nullptr;

	Model* model_ = nullptr;
	std::string modelName_;
	uint32_t textureSrvIndexOverride_ = UINT32_MAX;

	Camera* camera_ = nullptr;

	//3Dオブジェクト自身のトランスフォーム
	Transform transform_{};
	Matrix4x4 parentWorldMatrix_{};
	bool hasParentWorldMatrix_ = false;

	// 行列・Light・CameraをGPUへ渡すConstant Bufferをまとめた描画用データです。
	std::unique_ptr<Object3dGpuData> gpuData_;

	//アニメーション
	Model::Animation currentAnimation_;//アニメーション読み込み
	float animationTime_ = 0.0f;// アニメーションの再生時間を管理
	bool isAnimating_ = false;
	bool isLoop_ = true;//デフォルトはループ
	float maxPlayTime_ = 0.0f;
	ReturnPlaybackState animationReturnState_;
	TransformPlaybackController transformPlayback_;

	Model::Skeleton skeleton_; // このオブジェクト専用の骨
	bool isSkeletal_ = false;  // スケルトンを持っているかどうか
	Model::SkinCluster skinCluster_;//骨に合わせて動く体
};

