#pragma once

#include "Object3d.h"
#include <cstdint>
#include <vector>
#include <wrl.h>

namespace Engine { class Camera; }
namespace Engine { class DirectXCommon; }
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;

// Object3dが描画時にGPUへ渡す行列・Light・Camera・ReflectionのConstant Bufferを管理します。
// ModelのAnimationやTransformそのものは持たず、Object3dから受け取った値をGPU用データへ変換するだけです。
class Object3dGpuData
{
public:
	// 三つのConstant Bufferが完成した時だけ、Object3dへ成功を返します。
	bool Initialize(Engine::DirectXCommon* dxCommon);
	// World行列とCameraを使い、通常描画用の行列・Camera位置を更新します。
	void UpdateTransform(const Matrix4x4& worldMatrix, const Engine::Camera* camera);
	// 通常モデル描画で使う各Constant BufferをRoot Parameterへ設定します。
	bool BindForObjectDraw(ID3D12GraphicsCommandList* commandList);
	// 壁越しシルエット描画で使う行列と色をRoot Parameterへ設定します。
	void BindForOccludedSilhouetteDraw(
		ID3D12GraphicsCommandList* commandList,
		const Vector4& color) const;
	// 鏡描画で使う行列・Reflectionデータを描画ごとに固定し、Root Parameterへ設定します。
	bool BindForMirrorDraw(
		ID3D12GraphicsCommandList* commandList,
		const Matrix4x4& reflectionViewProjection,
		const Vector4& tint);

	// Sceneが決めたLightをこのObjectのCPU側へ保持し、Draw時にGPU用バッファへ固定します。
	void SetDirectionalLight(const Engine::Object3d::DirectionalLight& light) { directionalLightData_ = light; }
	void SetPointLight(const Engine::Object3d::PointLight& light) { pointLightData_ = light; }
	void SetSpotLight(const Engine::Object3d::SpotLight& light);
	void SetSpotLights(const std::array<Engine::Object3d::SpotLight, Engine::Object3d::kMaximumSpotLightCount>& lights);
	const Engine::Object3d::DirectionalLight& GetDirectionalLight() const { return directionalLightData_; }
	// Camera用バッファの空き成分で、Objectごとの裏面の明るさを渡します。
	void SetBackFaceBrightness(float brightness) { cameraData_->backFaceBrightness = brightness; }
	// 個体別の透明度をCamera用Constant Bufferへ渡します。
	void SetOpacity(float opacity) { cameraData_->opacity = opacity; }
	// 一体分の反射倍率と発光量を、通常描画と鏡面描画で同じ値へ固定します。
	void SetSpecularGlow(float specularBoost, float glowIntensity) {
		cameraData_->specularBoost = specularBoost;
		cameraData_->glowIntensity = glowIntensity;
	}
	// Playerの回転とは独立したLightの世界方向と警告の強さを描画へ渡します。
	void SetLightWarning(const Vector3& worldDirection, float strength) {
		cameraData_->warningDirection = worldDirection;
		cameraData_->warningStrength = strength;
	}

private:
	// GPUの描画が終わるまで行列・Camera・Lightを上書きしないよう、一回の描画に使う値を保持します。
	struct DrawConstants
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> transformResource;
		Engine::Object3d::TransformationMatrix* transformData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource;
		Engine::Object3d::CameraForGPU* cameraData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource;
		Engine::Object3d::DirectionalLight* directionalLightData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource;
		Engine::Object3d::PointLight* pointLightData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource;
		Engine::Object3d::SpotLightSet* spotLightData = nullptr;
	};
	// SwapChainの枠が再利用可能になるまで、描画ごとの行列・Camera・Lightを残します。
	struct FrameDrawConstants
	{
		uint64_t serial = 0;
		size_t nextDraw = 0;
		std::vector<DrawConstants> draws;
	};
	// 鏡面専用Pipelineが読む行列と反射Camera情報を、一回の描画が終わるまで保持します。
	struct MirrorDrawConstants
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> transformResource;
		Engine::Object3d::TransformationMatrix* transformData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> reflectionResource;
		Engine::Object3d::ReflectionData* reflectionData = nullptr;
	};
	// SwapChain枠のFence待ち後だけ、同じ鏡面用領域を再利用します。
	struct FrameMirrorConstants
	{
		uint64_t serial = 0;
		size_t nextDraw = 0;
		std::vector<MirrorDrawConstants> draws;
	};
	static constexpr size_t kFrameSlotCount = 2;
	// 描画一回分の行列・Camera・Lightバッファを作成します。
	bool CreateDrawConstants(DrawConstants& constants);
	// 鏡面描画一回分の行列と反射情報バッファを作成します。
	bool CreateMirrorDrawConstants(MirrorDrawConstants& constants);
	// 行列・Camera・壁越し表示用のBufferをそれぞれ作り、Map失敗を返します。
	bool CreateTransformationMatrixData(Engine::DirectXCommon* dxCommon);
	bool CreateCameraData(Engine::DirectXCommon* dxCommon);
	bool CreateOccludedSilhouetteData(Engine::DirectXCommon* dxCommon);
	// 初期化が途中で止まった時、所有するGPU資源とMap先を一緒に外します。
	void ResetResources();
	void InitializeDefaultLights();

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	Engine::Object3d::TransformationMatrix* transformationMatrixData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	Engine::Object3d::CameraForGPU* cameraData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> occludedSilhouetteResource_;
	Engine::Object3d::OccludedSilhouetteData* occludedSilhouetteData_ = nullptr;
	Engine::Object3d::DirectionalLight directionalLightData_{};
	Engine::Object3d::PointLight pointLightData_{};
	Engine::Object3d::SpotLightSet spotLightData_{};
	Engine::DirectXCommon* dxCommon_ = nullptr;
	FrameDrawConstants frameDrawConstants_[kFrameSlotCount]{};
	FrameMirrorConstants frameMirrorConstants_[kFrameSlotCount]{};
};
