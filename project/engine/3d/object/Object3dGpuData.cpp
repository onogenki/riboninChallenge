#include "Object3dGpuData.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "MyMath.h"
#include <cmath>
#include <numbers>
#include <utility>

using namespace MyMath;

// Object3d一体分のConstant Bufferを作成し、安全な初期値を書き込みます。
void Object3dGpuData::Initialize(DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
	CreateTransformationMatrixData(dxCommon);
	CreateCameraData(dxCommon);
	CreateOccludedSilhouetteData(dxCommon);
	CreateReflectionData(dxCommon);
	CreateDirectionalLightData(dxCommon);
	CreatePointLightData(dxCommon);
	CreateSpotLightData(dxCommon);
}

// World行列とCameraを使い、通常描画用の行列・Camera位置を更新します。
void Object3dGpuData::UpdateTransform(const Matrix4x4& worldMatrix, const Camera* camera)
{
	if (!transformationMatrixData_) {
		return;
	}

	transformationMatrixData_->World = worldMatrix;
	transformationMatrixData_->WorldInverseTranspose = Transpose(Inverse(worldMatrix));
	if (!camera || !cameraData_) {
		return;
	}
	transformationMatrixData_->WVP = Multiply(worldMatrix, camera->GetViewProjectionMatrix());
	cameraData_->worldPosition = camera->GetTranslate();
}

// 鏡と通常画面を同じFrameで描いても行列が上書きされないよう、描画ごとに値を固定します。
bool Object3dGpuData::BindForObjectDraw(ID3D12GraphicsCommandList* commandList)
{
	if (!commandList || !dxCommon_ || !transformationMatrixData_ || !cameraData_ ||
		!directionalLightResource_ || !pointLightResource_ || !spotLightResource_) {
		return false;
	}
	const UINT frameIndex = dxCommon_->GetFrameIndex();
	if (frameIndex >= kFrameSlotCount) {
		return false;
	}
	FrameMatrices& frame = frameMatrices_[frameIndex];
	const uint64_t serial = dxCommon_->GetFrameSerial();
	if (frame.serial != serial) {
		frame.serial = serial;
		frame.nextDraw = 0;
	}
	if (frame.nextDraw == frame.draws.size()) {
		DrawMatrices matrices;
		if (!CreateDrawMatrices(matrices)) {
			return false;
		}
		frame.draws.push_back(std::move(matrices));
	}
	DrawMatrices& matrices = frame.draws[frame.nextDraw++];
	*matrices.transformData = *transformationMatrixData_;
	*matrices.cameraData = *cameraData_;
	commandList->SetGraphicsRootConstantBufferView(
		1,
		matrices.transformResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		3,
		directionalLightResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		4,
		matrices.cameraResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		5,
		pointLightResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		6,
		spotLightResource_->GetGPUVirtualAddress());
	return true;
}

// 一回の描画専用Upload領域を確保し、Map失敗時は描画しないように知らせます。
bool Object3dGpuData::CreateDrawMatrices(DrawMatrices& matrices)
{
	matrices.transformResource = dxCommon_->CreateBufferResource(sizeof(Object3d::TransformationMatrix));
	matrices.cameraResource = dxCommon_->CreateBufferResource(sizeof(Object3d::CameraForGPU));
	if (!matrices.transformResource || !matrices.cameraResource) {
		return false;
	}
	if (FAILED(matrices.transformResource->Map(
		0, nullptr, reinterpret_cast<void**>(&matrices.transformData))) ||
		FAILED(matrices.cameraResource->Map(
			0, nullptr, reinterpret_cast<void**>(&matrices.cameraData)))) {
		return false;
	}
	return true;
}

// 壁越し表示では通常Lightを使わず、Objectの行列と指定色だけをGPUへ渡します。
void Object3dGpuData::BindForOccludedSilhouetteDraw(
	ID3D12GraphicsCommandList* commandList,
	const Vector4& color) const
{
	if (!commandList || !transformationMatrixResource_ ||
		!occludedSilhouetteResource_ || !occludedSilhouetteData_) {
		return;
	}

	occludedSilhouetteData_->color = color;
	commandList->SetGraphicsRootConstantBufferView(
		1,
		transformationMatrixResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		8,
		occludedSilhouetteResource_->GetGPUVirtualAddress());
}

// 鏡描画で使う行列・ReflectionデータをRoot Parameterへ設定します。
void Object3dGpuData::BindForMirrorDraw(
	ID3D12GraphicsCommandList* commandList,
	const Matrix4x4& reflectionViewProjection,
	const Vector4& tint) const
{
	if (!commandList || !transformationMatrixResource_ || !reflectionDataResource_ || !reflectionData_) {
		return;
	}
	reflectionData_->reflectionViewProjection = reflectionViewProjection;
	reflectionData_->tint = tint;
	commandList->SetGraphicsRootConstantBufferView(
		0,
		transformationMatrixResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		1,
		reflectionDataResource_->GetGPUVirtualAddress());
}

// 一灯だけを設定する既存Scene向けに、配列の先頭だけへSpotLightを入れます。
void Object3dGpuData::SetSpotLight(const Object3d::SpotLight& light)
{
	spotLightData_->lights.fill({});
	spotLightData_->lights[0] = light;
}

// 複数SpotLightをそのままGPU用配列へコピーします。
void Object3dGpuData::SetSpotLights(
	const std::array<Object3d::SpotLight, Object3d::kMaximumSpotLightCount>& lights)
{
	spotLightData_->lights = lights;
}

// Transform行列用Constant Bufferを確保します。
void Object3dGpuData::CreateTransformationMatrixData(DirectXCommon* dxCommon)
{
	transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(Object3d::TransformationMatrix));
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();
	transformationMatrixData_->WorldInverseTranspose = MakeIdentity4x4();
}

// Directional Light用Constant Bufferを確保します。
void Object3dGpuData::CreateDirectionalLightData(DirectXCommon* dxCommon)
{
	directionalLightResource_ = dxCommon->CreateBufferResource(sizeof(Object3d::DirectionalLight));
	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = Normalize({ 1.0f, -1.0f, 1.0f });
	directionalLightData_->intensity = 1.0f;
}

// Camera位置用Constant Bufferを確保します。
void Object3dGpuData::CreateCameraData(DirectXCommon* dxCommon)
{
	cameraResource_ = dxCommon->CreateBufferResource(sizeof(Object3d::CameraForGPU));
	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
	cameraData_->worldPosition = { 0.0f, 0.0f, 10.0f };
	cameraData_->backFaceBrightness = 1.0f;
}

// 壁越しシルエット専用の色を、Objectごとに変更できるConstant Bufferへ保存します。
void Object3dGpuData::CreateOccludedSilhouetteData(DirectXCommon* dxCommon)
{
	occludedSilhouetteResource_ = dxCommon->CreateBufferResource(sizeof(Object3d::OccludedSilhouetteData));
	occludedSilhouetteResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&occludedSilhouetteData_));
	occludedSilhouetteData_->color = { 0.35f, 0.85f, 1.0f, 0.65f };
}

// 鏡反射のViewProjectionと色用Constant Bufferを確保します。
void Object3dGpuData::CreateReflectionData(DirectXCommon* dxCommon)
{
	reflectionDataResource_ = dxCommon->CreateBufferResource(sizeof(Object3d::ReflectionData));
	reflectionDataResource_->Map(0, nullptr, reinterpret_cast<void**>(&reflectionData_));
	reflectionData_->reflectionViewProjection = MakeIdentity4x4();
	reflectionData_->tint = { 1.0f, 1.0f, 1.0f, 1.0f };
}

// Point Light用Constant Bufferを確保します。
void Object3dGpuData::CreatePointLightData(DirectXCommon* dxCommon)
{
	pointLightResource_ = dxCommon->CreateBufferResource(sizeof(Object3d::PointLight));
	pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));
	pointLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLightData_->position = { 0.0f, 2.0f, 0.0f };
	pointLightData_->intensity = 1.0f;
	pointLightData_->radius = 10.0f;
	pointLightData_->decay = 1.0f;
}

// Spot Light配列用Constant Bufferを確保します。
void Object3dGpuData::CreateSpotLightData(DirectXCommon* dxCommon)
{
	spotLightResource_ = dxCommon->CreateBufferResource(sizeof(Object3d::SpotLightSet));
	spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));
	spotLightData_->lights.fill({});
	Object3d::SpotLight& defaultLight = spotLightData_->lights[0];
	defaultLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	defaultLight.position = { 2.0f, 1.25f, 0.0f };
	defaultLight.distance = 7.0f;
	defaultLight.direction = Normalize({ -1.0f, -1.0f, 0.0f });
	defaultLight.intensity = 4.0f;
	defaultLight.decay = 2.0f;
	defaultLight.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
	defaultLight.cosFalloffStart = 1.0f;
}
