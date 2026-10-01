#pragma once
#include "DirectXCommon.h"
#include <wrl.h>
//スプライト共通部
class SpriteCommon
{
	public:

		static SpriteCommon* GetInstance() {
			static SpriteCommon instance;
			return &instance;
		}

		SpriteCommon(const SpriteCommon&) = delete;
		SpriteCommon& operator=(const SpriteCommon&) = delete;

	void Initialize(DirectXCommon* dxCommon);

	void SetCommonDrawSetting();

	ID3D12Device* GetDevice() const { return dxCommon_->GetDevice(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return dxCommon_->GetCommandList(); }

	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
	SpriteCommon() = default;
	~SpriteCommon() = default;

	DirectXCommon* dxCommon_ = nullptr;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	// グラフィックスパイプライン
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	//ルートシグネチャの作成
	void CreateRootSignature();
	//グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();

};

