#include "Object3d.h"
#include "DirectXCommon.h"
#include "Object3dCommon.h"
#include "Object3dGpuData.h"
#include "ModelManager.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include <cassert>
#include <algorithm>
#include "MyMath.h"
using namespace MyMath;

Object3d::Object3d() = default;

Object3d::~Object3d()
{
	ReleaseSkinClusterDescriptors();
}

void Object3d::Initialize(Object3dCommon* object3dCommon)
{//引数で受け取ってメンバ変数に記録する

	this->object3dCommon = object3dCommon;

	//デフォルトカメラを自分にセット
	this->camera = object3dCommon->GetDefaultCamera();

	// 描画用の行列・Light・Camera Constant Bufferは、専用のGPUデータ部品へまとめます。
	gpuData_ = std::make_unique<Object3dGpuData>();
	gpuData_->Initialize(object3dCommon->GetDxCommon());
	//Transform変数を作る
	transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	transformPlayback_.Reset();
	animationReturnState_.Reset();
}

void Object3d::Update()
{
	const float deltaTime = DirectXCommon::GetInstance()->GetDeltaTime();
	transformPlayback_.Update(transform, deltaTime);

	if (model_ && isAnimating_ && currentAnimation_.duration > 0.0f)
	{// 実時間を 1/60 秒ずつ進める
		if (animationReturnState_.IsReturning()) {
			animationTime_ -= deltaTime;
			if (animationTime_ <= 0.0f) {
				animationTime_ = 0.0f;
				animationReturnState_.Reset();
			}
		} else {
			animationTime_ += deltaTime;
		}

		// ループOFFなら、自動的にアニメーションの1本分の長さ（duration)にする
		if (!isLoop_ && maxPlayTime_ == 0.0f) {
			maxPlayTime_ = currentAnimation_.duration;
		}

		//時間指定されてる場合の止まる処理
		if (!animationReturnState_.IsReturning() && maxPlayTime_ > 0.0f)
		{
			if (animationTime_ >= maxPlayTime_)
			{
				animationTime_ = maxPlayTime_;//指定時間でタイマー固定
			}
		}

		// 実際にモデルを動かすループ時間を計算
		float finalRenderTime = std::fmod(animationTime_, currentAnimation_.duration);

		// もし制限時間に達して止まったなら、最後のポーズで固定する
		if (maxPlayTime_ > 0.0f && animationTime_ >= maxPlayTime_) {
			finalRenderTime = std::fmod(maxPlayTime_, currentAnimation_.duration);
		}
		//アニメーションを骨に適用して行列をアップデート
		model_->ApplyAnimation(skeleton_, currentAnimation_, finalRenderTime);
		//親子関係を計算してskeletonSpaceMatrixを更新
		model_->Update(skeleton_);
		model_->Update(skinCluster_, skeleton_);
	}
	//TransformからWorldMatrixを作る
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	if (hasParentWorldMatrix_)
	{
		worldMatrix = Multiply(worldMatrix, parentWorldMatrix_);
	}

	if (gpuData_) {
		gpuData_->UpdateTransform(worldMatrix, camera);
	}
}

// 指定名のJointが存在すれば、現在のモデルWorldを含むJoint行列を返します。
bool Object3d::GetJointWorldMatrix(const std::string& jointName, Matrix4x4& worldMatrix) const
{
	if (!isSkeletal_)
	{
		return false;
	}

	const auto jointIterator = skeleton_.jointMap.find(jointName);
	if (jointIterator == skeleton_.jointMap.end())
	{
		return false;
	}

	const Matrix4x4 modelWorldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	worldMatrix = Multiply(skeleton_.joints[jointIterator->second].skeletonSpaceMatrix, modelWorldMatrix);
	return true;
}

// このObject3dのWorld行列へ親のWorld行列を掛け、手持ちWeaponなどを追従させます。
void Object3d::SetParentWorldMatrix(const Matrix4x4& parentWorldMatrix)
{
	parentWorldMatrix_ = parentWorldMatrix;
	hasParentWorldMatrix_ = true;
}

// 親子付けを解除し、Object自身のTransformだけでWorld行列を作る状態へ戻します。
void Object3d::ClearParentWorldMatrix()
{
	parentWorldMatrix_ = MakeIdentity4x4();
	hasParentWorldMatrix_ = false;
}

// 一フレーム前のTransformを、現在のTransformへ戻す履歴として記録します。
void Object3d::RecordTransformEdit(const Transform& before)
{
	RecordTransformEdit(before, DirectXCommon::GetInstance()->GetDeltaTime());
}

// 指定秒数を使い、Transform編集の履歴を記録します。
void Object3d::RecordTransformEdit(const Transform& before, float elapsedSeconds)
{
	transformPlayback_.RecordEdit(before, transform, elapsedSeconds);
}

// Animation時間を進めず、反射Cameraなど指定Camera用のWVPだけを更新します。
void Object3d::UpdateCameraForDraw(Camera* drawCamera)
{
	if (!drawCamera) {
		return;
	}
	camera = drawCamera;
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	if (hasParentWorldMatrix_) {
		worldMatrix = Multiply(worldMatrix, parentWorldMatrix_);
	}
	if (gpuData_) {
		gpuData_->UpdateTransform(worldMatrix, camera);
	}
}

// 自分の頂点形式に合う描画設定を選び、直前のモデルの種類に依存せず描画します。
void Object3d::Draw()
{
	if (!model_) {
		return;
	}

	ID3D12GraphicsCommandList* commandList = object3dCommon->GetDxCommon()->GetCommandList();
	if (isSkeletal_) {
		// 骨ありModelは、元から使っていたVertex Shader方式で骨行列と頂点影響度を渡して描きます。
		// これによりwalk.gltfなどのAnimation Modelを、通常Modelと別の正しい頂点形式で描画します。
		object3dCommon->SetSkinningCommonDrawSetting();
		if (!gpuData_ || !gpuData_->BindForObjectDraw(commandList)) {
			return;
		}

		const std::string& environmentTexturePath = object3dCommon->GetEnvironmentTexturePath();
		if (!environmentTexturePath.empty() && GetEnvironmentCoefficient() > 0.0f) {
			commandList->SetGraphicsRootDescriptorTable(
				8,
				TextureManager::GetInstance()->GetSrvHandleGPU(environmentTexturePath));
		}
		model_->Draw(skinCluster_, textureSrvIndexOverride_);
		return;
	}

	// Animation・SkyBox・鏡の直後でも、通常モデル用の頂点形式とRoot Signatureへ戻します。
	// Root Signatureを切り替えた後で、このObjectの行列・Light・Textureを設定します。
	object3dCommon->SetCommonDrawSetting();
	if (!gpuData_ || !gpuData_->BindForObjectDraw(commandList)) {
		return;
	}
	const std::string& environmentTexturePath = object3dCommon->GetEnvironmentTexturePath();
	if (!environmentTexturePath.empty() && GetEnvironmentCoefficient() > 0.0f) {
		commandList->SetGraphicsRootDescriptorTable(
			7,
			TextureManager::GetInstance()->GetSrvHandleGPU(environmentTexturePath));
	}
	model_->Draw(textureSrvIndexOverride_);
}

// 通常の深度値より奥にある面だけを描き、壁に隠れたObjectの形だけを見せます。
void Object3d::DrawOccludedSilhouette(const Vector4& color)
{
	if (!model_ || !object3dCommon || !gpuData_) {
		return;
	}

	ID3D12GraphicsCommandList* commandList = object3dCommon->GetDxCommon()->GetCommandList();
	if (isSkeletal_) {
		// 骨ありModelも通常描画と同じ変形済み頂点を使うため、Animationの形がそのまま残ります。
		object3dCommon->SetSkinningComputeSetting();
		model_->DispatchSkinning(skinCluster_);
	}

	object3dCommon->SetOccludedSilhouetteDrawSetting();
	gpuData_->BindForOccludedSilhouetteDraw(commandList, color);
	if (isSkeletal_) {
		model_->DrawSkinned(skinCluster_, textureSrvIndexOverride_);
		return;
	}
	model_->Draw(textureSrvIndexOverride_);
}
// 反射Textureを鏡面用Pipelineで描画し、通常モデル描画とは別のRoot Parameterを設定します。
void Object3d::DrawMirror(
	uint32_t reflectionTextureSrvIndex,
	const Matrix4x4& reflectionViewProjection)
{
	if (!model_ || reflectionTextureSrvIndex == UINT32_MAX || !gpuData_) {
		return;
	}

	ID3D12GraphicsCommandList* commandList = object3dCommon->GetDxCommon()->GetCommandList();
	object3dCommon->SetMirrorDrawSetting();
	gpuData_->BindForMirrorDraw(
		commandList,
		reflectionViewProjection,
		{ 0.96f, 0.98f, 1.0f, 1.0f });
	commandList->SetGraphicsRootDescriptorTable(
		2,
		SrvManager::GetInstance()->GetGPUDescriptorHandle(reflectionTextureSrvIndex));
	model_->DrawGeometry();
}

// Factoryなどが読込み済みモデルを名前で探し、このObject3dの見た目として設定します。
void Object3d::SetModel(const std::string& filePath)
{
	// モデルマネージャからモデルを検索してセットする
	model_ = ModelManager::GetInstance()->FindModel(filePath);
	modelName_ = filePath;
}

// Model本来のTextureの代わりに、指定TextureをこのObjectだけへ適用します。
void Object3d::SetTextureOverride(const std::string& texturePath)
{
	TextureManager::GetInstance()->LoadTexture(texturePath);
	textureSrvIndexOverride_ = TextureManager::GetInstance()->GetSrvIndex(texturePath);
}

// 共用の材質ではなくObject専用の値を変更し、他のplane.objへ影響させません。
void Object3d::SetBackFaceBrightness(float brightness)
{
	if (gpuData_) {
		gpuData_->SetBackFaceBrightness(std::clamp(brightness, 0.0f, 1.0f));
	}
}

// Sceneから渡された平行光を、このObject3dのGPU描画データへ設定します。
void Object3d::SetDirectionalLight(const DirectionalLight& light)
{
	if (gpuData_) {
		gpuData_->SetDirectionalLight(light);
	}
}

// Sceneから渡された点光源を、このObject3dのGPU描画データへ設定します。
void Object3d::SetPointLight(const PointLight& light)
{
	if (gpuData_) {
		gpuData_->SetPointLight(light);
	}
}

// Sceneから渡されたSpotLight一灯を、このObject3dのGPU描画データへ設定します。
void Object3d::SetSpotLight(const SpotLight& light)
{
	if (gpuData_) {
		gpuData_->SetSpotLight(light);
	}
}

// Sceneから渡された複数のSpotLightを、このObject3dのGPU描画データへ設定します。
void Object3d::SetSpotLights(const std::array<SpotLight, kMaximumSpotLightCount>& lights)
{
	if (gpuData_) {
		gpuData_->SetSpotLights(lights);
	}
}

// Inspectorなどが現在の平行光設定を読むため、GPU描画データから取得します。
const Object3d::DirectionalLight& Object3d::GetDirectionalLight() const
{
	static const DirectionalLight kDefaultLight{};
	return gpuData_ ? gpuData_->GetDirectionalLight() : kDefaultLight;
}

// SkeletalモデルだけにSkinClusterとSkeletonを作り、Animation再生できる状態へ初期化します。
void Object3d::InitializeAnimation()
{
	//既に同じモデルがセットされている場合は、再生成を防ぐために何もせず抜ける
	if (!model_)return;
	ReleaseSkinClusterDescriptors();
	if (model_->GetModelData().skinClusterData.empty()) {
		skeleton_ = {};
		skinCluster_ = {};
		isSkeletal_ = false;
		isAnimating_ = false;
		return;
	}

	//モデルの階層構造からスケルトンを生成
	skeleton_ = model_->CreateSkeleton(model_->GetModelData().rootNode);
	//ジョイントがあればスケルトンモデルとして扱う
	isSkeletal_ = !skeleton_.joints.empty();

	if (isSkeletal_) {
		constexpr uint32_t kSkinClusterDescriptorCount = 4;
		if (!SrvManager::GetInstance()->CanAllocate(kSkinClusterDescriptorCount)) {
			skeleton_ = {};
			skinCluster_ = {};
			isSkeletal_ = false;
			isAnimating_ = false;
			return;
		}
		//スケルトンがあるならSkinClusterも作成する
		// （引数の descriptorHeap や device は DxCommon 等から引っ張ってくる）
		auto device = object3dCommon->GetDxCommon()->GetDevice();
		auto srvHeap = SrvManager::GetInstance()->GetDescriptorHeap();
		uint32_t descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		skinCluster_ = model_->CreateSkinCluster(device, skeleton_, model_->GetModelData(), srvHeap, descriptorSize);
		model_->Update(skeleton_);
		model_->Update(skinCluster_, skeleton_);
	}
	else
	{
		isSkeletal_ = false;//アニメーションしないモデルはfalse
	}
}

// SkinClusterが確保したSRV/UAVの番号を返却し、再初期化や破棄後に残さないようにします。
void Object3d::ReleaseSkinClusterDescriptors()
{
	auto freeIndex = [](uint32_t& index) {
		if (index != UINT32_MAX) {
			SrvManager::GetInstance()->Free(index);
			index = UINT32_MAX;
		}
	};

	freeIndex(skinCluster_.paletteSrvIndex);
	freeIndex(skinCluster_.inputVertexSrvIndex);
	freeIndex(skinCluster_.influenceSrvIndex);
	freeIndex(skinCluster_.outputVertexUavIndex);
	skinCluster_ = {};
}

// 指定AnimationをこのObjectへ設定し、再生時間を先頭へ戻します。
void Object3d::PlayAnimation(const Model::Animation& animation)
{
	currentAnimation_ = animation;
	animationTime_ = 0.0f;
	animationReturnState_.Reset();
	isAnimating_ = true;
	if (model_ && isSkeletal_) {
		model_->BuildAnimationMapping(skeleton_, currentAnimation_);
	}
}

// モデルの環境反射の強さを設定します。
void Object3d::SetEnvironmentCoefficient(float coefficient) {
	if (model_) {
		model_->SetEnvironmentCoefficient(coefficient);
	}
}

// モデルに設定された環境反射の強さを返します。
float Object3d::GetEnvironmentCoefficient() const {
	if (model_) {
		return model_->GetEnvironmentCoefficient();
	}
	return 0.0f;
}
