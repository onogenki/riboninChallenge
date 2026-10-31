#pragma once
#include "DirectXCommon.h"
#include <wrl.h>

namespace Engine
{

// 全Spriteで共用するRoot Signatureと描画Pipelineを所有します。
// Spriteごとの位置・色・Textureは持たず、Draw前の共通設定だけを行います。
class SpriteCommon
{
public:

	// Sceneが共有する本体への、所有権のないポインタを返します。
	static SpriteCommon* GetInstance() {
		static SpriteCommon instance;
		return &instance;
	}

	SpriteCommon(const SpriteCommon&) = delete;
	SpriteCommon& operator=(const SpriteCommon&) = delete;

	// SceneからDirectX操作窓口を借り、共通描画設定を完成できた時だけtrueを返します。
	bool Initialize(Engine::DirectXCommon* dxCommon);

	// Sprite::Drawより先に共通Pipelineと三角形描画を設定します。
	void SetCommonDrawSetting();
	// 最後に重ねる白フェードだけ、既存の深度を変更せず描画します。
	void SetOverlayDrawSetting();

	// GPU資源作成用のDeviceと、描画命令を書き込むCommand Listを返します。
	ID3D12Device* GetDevice() const { return dxCommon_->GetDevice(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return dxCommon_->GetCommandList(); }

	// Sceneから借りているDirectX操作窓口を、所有権のないポインタで返します。
	Engine::DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
	SpriteCommon() = default;
	~SpriteCommon() = default;

	Engine::DirectXCommon* dxCommon_ = nullptr;

	// ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	// グラフィックスパイプライン
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> overlayPipelineState_;

	//ルートシグネチャの作成
	void CreateRootSignature();
	//グラフィックスパイプラインの生成
	void CreateGraphicsPipeline();

};

} // namespace Engine

