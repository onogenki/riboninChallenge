#pragma once
#include "DirectXCommon.h"
#include "Camera.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <string>

// 通常モデル・骨付きモデル・鏡・SkyBoxに共通の描画Pipelineを所有します。
// 個々のObject3dの位置やAnimation状態は持たず、描画時に必要な設定を切り替えます。
class Object3dCommon
{
public:

	// Sceneが共有する描画設定本体への、所有権のないポインタを返します。
	static Object3dCommon* GetInstance() {
		static Object3dCommon instance;
		return &instance;
	}

	Object3dCommon(const Object3dCommon&) = delete;
	Object3dCommon& operator=(const Object3dCommon&) = delete;

	// SceneからDirectX操作窓口を借り、通常・骨付き・鏡・SkyBox用Pipelineを作ります。
	void Initialize(DirectXCommon* dxCommon);

	// GPU資源作成に使う、所有権のないDirectX操作窓口を返します。
	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	// Sceneが設定した既定Cameraへの、所有権のないポインタを返します。
	Engine::Camera* GetDefaultCamera() const { return defaultCamera_; }
	// 環境反射に使用するTextureのパスを、変更せず参照できる形で返します。
	const std::string& GetEnvironmentTexturePath() const { return environmentTexturePath_; }

	// 通常モデル用のRoot Signature・Pipeline・三角形描画を設定します。
	void SetCommonDrawSetting();
	// 個体別Opacityが1未満の通常モデルだけ、深度を書かない透過Pipelineへ切り替えます。
	void SetTransparentDrawSetting();
	// 壁の奥だけにObjectの半透明シルエットを描く共通Pipelineを設定します。
	void SetOccludedSilhouetteDrawSetting();
	// 骨付きモデルの壁越しシルエット用に頂点を変形するCompute Pipelineを設定します。
	void SetSkinningComputeSetting();
	// 反射Textureを平面へ投影する固定鏡専用の描画設定へ切り替えます。
	void SetMirrorDrawSetting();

	// Scene所有のCameraを既定値として借ります。Cameraの寿命はScene側が管理します。
	void SetDefaultCamera(Engine::Camera* camera) { this->defaultCamera_ = camera; }
	// 環境反射用Textureのパスを保存します。空文字ならObject3dは環境反射を指定しません。
	void SetEnvironmentTexturePath(const std::string& path) {
		environmentTexturePath_ = path;
	}

	// 骨付きモデルのVertex Shader描画用Pipelineへ切り替えます。
	void SetSkinningCommonDrawSetting();

	// このクラスが所有するSkyBox用設定への、所有権のないポインタを返します。
	ID3D12RootSignature* GetSkyboxRootSignature() const { return skyboxRootSignature_.Get(); }
	ID3D12PipelineState* GetSkyboxPipelineState() const { return skyboxGraphicsPipelineState_.Get(); }

private:

	Object3dCommon() = default;
	~Object3dCommon() = default;

	DirectXCommon* dxCommon_ = nullptr;

	Engine::Camera* defaultCamera_ = nullptr;

	std::string environmentTexturePath_ = "";

	// ルートシグネチャの生成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();
	void CreateOccludedSilhouetteGraphicsPipeline();
	void CreateMirrorRootSignature();
	void CreateMirrorGraphicsPipeline();

	// RootSignature
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	// PipelineState
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> transparentGraphicsPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> occludedSilhouetteGraphicsPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mirrorRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mirrorGraphicsPipelineState_;

	// スキニング用のルートシグネチャとパイプライン
	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningGraphicsPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningComputeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningComputePipelineState_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> skyboxRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skyboxGraphicsPipelineState_;

	// スキニング用の生成関数
	void CreateSkinningRootSignature();
	void CreateSkinningGraphicsPipeline();
	void CreateSkinningComputeRootSignature();
	void CreateSkinningComputePipeline();

	//Skybox用生成関数
	void CreateSkyboxRootSignature();
	void CreateSkyboxPipeline();

};

