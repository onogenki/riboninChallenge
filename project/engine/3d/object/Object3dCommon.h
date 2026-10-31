#pragma once
#include "DirectXCommon.h"
#include "Camera.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <string>

namespace Engine
{
// 通常モデル・骨付きモデル・鏡・SkyBoxに共通の描画Pipelineを所有します。
// 個々のObject3dの位置やAnimation状態は持たず、描画時に必要な設定を切り替えます。
class Object3dCommon
{
public:

	// @brief 全Sceneで共有する描画設定本体を取得します。GPU資源の生成はInitializeで行います。
	// @return このクラスが所有する本体へのポインタです。呼び出し側は解放しません。
	static Object3dCommon* GetInstance() {
		static Object3dCommon instance;
		return &instance;
	}

	Object3dCommon(const Object3dCommon&) = delete;
	Object3dCommon& operator=(const Object3dCommon&) = delete;

	// @brief 通常・骨付き・鏡・SkyBox用Pipelineを生成し、後続の描画で共用します。
	// @param dxCommon 初期化済みのDirectX操作窓口です。このクラスは所有しません。
	// nullやDevice未準備、Pipeline生成失敗では例外を投げ、未完成の設定で描画を始めません。
	void Initialize(Engine::DirectXCommon* dxCommon);

	// @brief GPU資源作成に使うDirectX操作窓口を取得します。
	// @return DirectXCommonの共有インスタンスへの非所有ポインタです。Initialize前はnullです。
	Engine::DirectXCommon* GetDxCommon() const { return dxCommon_; }
	// @brief 共通描画で使う既定Cameraを取得します。
	// @return Sceneが設定したCameraへの非所有ポインタです。未設定ならnullです。
	Engine::Camera* GetDefaultCamera() const { return defaultCamera_; }
	// @brief 環境反射用Textureのパスを取得します。
	// @return このクラスが保持するパスへの読み取り専用参照です。未設定なら空文字です。
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

	// @brief Scene所有のCameraを共通描画の既定値として登録します。
	// @param camera このクラスは所有しません。切替先Sceneの初期化時に登録し直します。
	void SetDefaultCamera(Engine::Camera* camera) { this->defaultCamera_ = camera; }
	// @brief 環境反射用Textureのパスを保存します。空文字なら環境反射を指定しません。
	// @param path TextureManagerへ登録済みのパス、または環境反射を無効にする空文字です。
	void SetEnvironmentTexturePath(const std::string& path) {
		environmentTexturePath_ = path;
	}

	// 骨付きモデルのVertex Shader描画用Pipelineへ切り替えます。
	void SetSkinningCommonDrawSetting();

	// @brief SkyBox用のRoot Signatureを取得します。
	// @return このクラスが所有する設定への非所有ポインタです。Initialize前はnullです。
	ID3D12RootSignature* GetSkyboxRootSignature() const { return skyboxRootSignature_.Get(); }
	// @brief SkyBox用のPipeline Stateを取得します。
	// @return このクラスが所有する設定への非所有ポインタです。Initialize前はnullです。
	ID3D12PipelineState* GetSkyboxPipelineState() const { return skyboxGraphicsPipelineState_.Get(); }

private:

	Object3dCommon() = default;
	~Object3dCommon() = default;

	Engine::DirectXCommon* dxCommon_ = nullptr;

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
} // namespace Engine

