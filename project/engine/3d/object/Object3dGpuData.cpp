#include "Object3dGpuData.h"

#include "Camera.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include "MyMath.h"
#include <cmath>
#include <numbers>
#include <utility>

using namespace MyMath;

// 行列・Camera・輪郭用Bufferをそろえてから、Lightの初期値を使える状態にします。
bool Object3dGpuData::Initialize(DirectXCommon* dxCommon)
{
	ResetResources();
	if (!dxCommon || !dxCommon->GetDevice()) {
		Logger::Log("Object3d GPU data initialization failed: DirectX device is unavailable.");
		return false;
	}
	dxCommon_ = dxCommon;
	if (!CreateTransformationMatrixData(dxCommon) ||
		!CreateCameraData(dxCommon) ||
		!CreateOccludedSilhouetteData(dxCommon)) {
		ResetResources();
		return false;
	}
	InitializeDefaultLights();
	return true;
}

// 途中まで作った一体分のBufferとCPU側のMap先を外し、未完成状態を残しません。
void Object3dGpuData::ResetResources()
{
	transformationMatrixData_ = nullptr;
	cameraData_ = nullptr;
	occludedSilhouetteData_ = nullptr;
	transformationMatrixResource_.Reset();
	cameraResource_.Reset();
	occludedSilhouetteResource_.Reset();
	for (FrameDrawConstants& frame : frameDrawConstants_) {
		frame.draws.clear();
		frame.serial = 0;
		frame.nextDraw = 0;
	}
	for (FrameMirrorConstants& frame : frameMirrorConstants_) {
		frame.draws.clear();
		frame.serial = 0;
		frame.nextDraw = 0;
	}
	dxCommon_ = nullptr;
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

// GPUが前の描画を読む間も行列・Camera・Lightを上書きしないよう、描画ごとに値を固定します。
bool Object3dGpuData::BindForObjectDraw(ID3D12GraphicsCommandList* commandList)
{
	if (!commandList || !dxCommon_ || !transformationMatrixData_ || !cameraData_) {
		return false;
	}
	const UINT frameIndex = dxCommon_->GetFrameIndex();
	if (frameIndex >= kFrameSlotCount) {
		return false;
	}
	FrameDrawConstants& frame = frameDrawConstants_[frameIndex];
	const uint64_t serial = dxCommon_->GetFrameSerial();
	if (frame.serial != serial) {
		frame.serial = serial;
		frame.nextDraw = 0;
	}
	if (frame.nextDraw == frame.draws.size()) {
		DrawConstants constants;
		if (!CreateDrawConstants(constants)) {
			return false;
		}
		frame.draws.push_back(std::move(constants));
	}
	DrawConstants& constants = frame.draws[frame.nextDraw++];
	*constants.transformData = *transformationMatrixData_;
	*constants.cameraData = *cameraData_;
	*constants.directionalLightData = directionalLightData_;
	*constants.pointLightData = pointLightData_;
	*constants.spotLightData = spotLightData_;
	// Root Parameter 3・5・6はPixel Shaderのb1・b3・b4へ対応し、三種類のLightを各Objectへ渡します。
	// 各値を描画ごとに固定し、鏡Cameraと通常Camera、前後フレームの値を混ぜません。
	commandList->SetGraphicsRootConstantBufferView(
		1,
		constants.transformResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		3,
		constants.directionalLightResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		4,
		constants.cameraResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		5,
		constants.pointLightResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		6,
		constants.spotLightResource->GetGPUVirtualAddress());
	return true;
}

// 一回の描画専用Upload領域を確保し、Map失敗時は描画しないように知らせます。
bool Object3dGpuData::CreateDrawConstants(DrawConstants& constants)
{
	constants.transformResource = dxCommon_->CreateBufferResource(sizeof(Object3d::TransformationMatrix));
	constants.cameraResource = dxCommon_->CreateBufferResource(sizeof(Object3d::CameraForGPU));
	constants.directionalLightResource = dxCommon_->CreateBufferResource(sizeof(Object3d::DirectionalLight));
	constants.pointLightResource = dxCommon_->CreateBufferResource(sizeof(Object3d::PointLight));
	constants.spotLightResource = dxCommon_->CreateBufferResource(sizeof(Object3d::SpotLightSet));
	if (!constants.transformResource || !constants.cameraResource ||
		!constants.directionalLightResource || !constants.pointLightResource ||
		!constants.spotLightResource) {
		return false;
	}
	if (FAILED(constants.transformResource->Map(
		0, nullptr, reinterpret_cast<void**>(&constants.transformData))) ||
		FAILED(constants.cameraResource->Map(
			0, nullptr, reinterpret_cast<void**>(&constants.cameraData))) ||
		FAILED(constants.directionalLightResource->Map(
			0, nullptr, reinterpret_cast<void**>(&constants.directionalLightData))) ||
		FAILED(constants.pointLightResource->Map(
			0, nullptr, reinterpret_cast<void**>(&constants.pointLightData))) ||
		FAILED(constants.spotLightResource->Map(
			0, nullptr, reinterpret_cast<void**>(&constants.spotLightData)))) {
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

// 前の鏡面描画をGPUが読む間に値を変えないよう、行列と反射情報を描画ごとに固定します。
bool Object3dGpuData::BindForMirrorDraw(
	ID3D12GraphicsCommandList* commandList,
	const Matrix4x4& reflectionViewProjection,
	const Vector4& tint)
{
	if (!commandList || !dxCommon_ || !transformationMatrixData_) {
		return false;
	}
	const UINT frameIndex = dxCommon_->GetFrameIndex();
	if (frameIndex >= kFrameSlotCount) {
		return false;
	}
	FrameMirrorConstants& frame = frameMirrorConstants_[frameIndex];
	const uint64_t serial = dxCommon_->GetFrameSerial();
	if (frame.serial != serial) {
		frame.serial = serial;
		frame.nextDraw = 0;
	}
	if (frame.nextDraw == frame.draws.size()) {
		MirrorDrawConstants constants;
		if (!CreateMirrorDrawConstants(constants)) {
			return false;
		}
		frame.draws.push_back(std::move(constants));
	}
	MirrorDrawConstants& constants = frame.draws[frame.nextDraw++];
	*constants.transformData = *transformationMatrixData_;
	constants.reflectionData->reflectionViewProjection = reflectionViewProjection;
	constants.reflectionData->tint = tint;
	commandList->SetGraphicsRootConstantBufferView(
		0,
		constants.transformResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(
		1,
		constants.reflectionResource->GetGPUVirtualAddress());
	return true;
}

// 鏡面一回分のUpload領域を作り、どちらかの確保・Map失敗時は描画しないようにします。
bool Object3dGpuData::CreateMirrorDrawConstants(MirrorDrawConstants& constants)
{
	constants.transformResource = dxCommon_->CreateBufferResource(sizeof(Object3d::TransformationMatrix));
	constants.reflectionResource = dxCommon_->CreateBufferResource(sizeof(Object3d::ReflectionData));
	if (!constants.transformResource || !constants.reflectionResource) {
		return false;
	}
	if (FAILED(constants.transformResource->Map(
		0, nullptr, reinterpret_cast<void**>(&constants.transformData))) ||
		FAILED(constants.reflectionResource->Map(
			0, nullptr, reinterpret_cast<void**>(&constants.reflectionData)))) {
		return false;
	}
	return true;
}

// 一灯だけを設定する既存Scene向けに、配列の先頭だけへSpotLightを入れます。
void Object3dGpuData::SetSpotLight(const Object3d::SpotLight& light)
{
	spotLightData_.lights.fill({});
	spotLightData_.lights[0] = light;
}

// 複数SpotLightをそのままGPU用配列へコピーします。
void Object3dGpuData::SetSpotLights(
	const std::array<Object3d::SpotLight, Object3d::kMaximumSpotLightCount>& lights)
{
	spotLightData_.lights = lights;
}

// Transform行列用Bufferの生成とMapを確認し、失敗した行列へ書き込みません。
bool Object3dGpuData::CreateTransformationMatrixData(DirectXCommon* dxCommon)
{
	transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(Object3d::TransformationMatrix));
	if (!transformationMatrixResource_ ||
		FAILED(transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_))) ||
		!transformationMatrixData_) {
		Logger::Log("Object3d GPU data initialization failed: transform buffer creation or mapping failed.");
		return false;
	}
	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();
	transformationMatrixData_->WorldInverseTranspose = MakeIdentity4x4();
	return true;
}

// Camera位置用Bufferの生成とMapを確認し、失敗した値へ書き込みません。
bool Object3dGpuData::CreateCameraData(DirectXCommon* dxCommon)
{
	cameraResource_ = dxCommon->CreateBufferResource(sizeof(Object3d::CameraForGPU));
	if (!cameraResource_ ||
		FAILED(cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_))) ||
		!cameraData_) {
		Logger::Log("Object3d GPU data initialization failed: camera buffer creation or mapping failed.");
		return false;
	}
	cameraData_->worldPosition = { 0.0f, 0.0f, 10.0f };
	cameraData_->backFaceBrightness = 1.0f;
	cameraData_->opacity = 1.0f;
	cameraData_->specularBoost = 1.0f;
	cameraData_->glowIntensity = 0.0f;
	cameraData_->padding = 0.0f;
	cameraData_->warningDirection = { 0.0f, 0.0f, 1.0f };
	cameraData_->warningStrength = 0.0f;
	return true;
}

// 壁越しシルエット用Bufferの生成とMapを確認し、失敗した色へ書き込みません。
bool Object3dGpuData::CreateOccludedSilhouetteData(DirectXCommon* dxCommon)
{
	occludedSilhouetteResource_ = dxCommon->CreateBufferResource(sizeof(Object3d::OccludedSilhouetteData));
	if (!occludedSilhouetteResource_ ||
		FAILED(occludedSilhouetteResource_->Map(
			0, nullptr, reinterpret_cast<void**>(&occludedSilhouetteData_))) ||
		!occludedSilhouetteData_) {
		Logger::Log("Object3d GPU data initialization failed: silhouette buffer creation or mapping failed.");
		return false;
	}
	occludedSilhouetteData_->color = { 0.35f, 0.85f, 1.0f, 0.65f };
	return true;
}

// SceneがLightを渡す前も従来の明るさで描けるよう、三種類の初期値をCPU側へ設定します。
void Object3dGpuData::InitializeDefaultLights()
{
	directionalLightData_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_.direction = Normalize({ 1.0f, -1.0f, 1.0f });
	directionalLightData_.intensity = 1.0f;
	pointLightData_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLightData_.position = { 0.0f, 2.0f, 0.0f };
	pointLightData_.intensity = 1.0f;
	pointLightData_.radius = 10.0f;
	pointLightData_.decay = 1.0f;
	spotLightData_.lights.fill({});
	Object3d::SpotLight& defaultLight = spotLightData_.lights[0];
	defaultLight.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	defaultLight.position = { 2.0f, 1.25f, 0.0f };
	defaultLight.distance = 7.0f;
	defaultLight.direction = Normalize({ -1.0f, -1.0f, 0.0f });
	defaultLight.intensity = 4.0f;
	defaultLight.decay = 2.0f;
	defaultLight.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
	defaultLight.cosFalloffStart = 1.0f;
}
