#pragma once

#include "Laser.h"
#include "Matrix4x4.h"
#include "Vector4.h"
#include <cstddef>
#include <cstdint>
#include <d3d12.h>
#include <vector>
#include <wrl.h>

class Camera;
class DirectXCommon;

// Laserが計算した線分を、ゲーム本体の3D空間へ描画します。
class LaserRenderer
{
public:
	bool Initialize(DirectXCommon* dxCommon, size_t maximumSegmentCount = 32);
	void Draw(const std::vector<LaserSegment>& segments, const Camera& camera);
	// 3D空間における光線の幅を設定する。
	void SetBeamWidth(float width) { beamWidth_ = width; }
	// 背景と区別しやすい色へ、Laser全体の描画色を設定する。
	void SetColor(const Vector4& color) { color_ = color; }

private:
	// DirectXCommonの二枚のSwapChainと、鏡・通常画面の二回描画に対応します。
	static constexpr size_t kFrameSlotCount = 2;
	static constexpr size_t kInitialDrawSlotsPerFrame = 2;
	struct Vertex
	{
		Vector4 position;
		// 光の帯の左右端を0と1で渡し、PixelShaderで中心と縁の透明度を変えます。
		float texcoordX = 0.0f;
		float texcoordY = 0.0f;
	};

	struct ConstantData
	{
		Matrix4x4 viewProjection;
		Vector4 color;
	};
	// 一回のDraw専用のUpload領域です。鏡と通常画面が同じ値を上書きしないようにします。
	struct DrawResources
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> constantResource;
		Vertex* mappedVertices = nullptr;
		ConstantData* mappedConstantData = nullptr;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	};
	// 同じSwapChain枠のFenceが終わるまで、前のフレームの描画データを保持します。
	struct FrameResources
	{
		uint64_t serial = 0;
		size_t nextDraw = 0;
		std::vector<DrawResources> draws;
	};

	bool CreateDrawResources(DrawResources& resources);
	bool CreateRootSignature();
	bool CreateGraphicsPipeline();

	DirectXCommon* dxCommon_ = nullptr;
	size_t maximumSegmentCount_ = 32;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
	FrameResources frameResources_[kFrameSlotCount]{};
	Vector4 color_{ 1.0f, 0.05f, 0.02f, 1.0f };
	float beamWidth_ = 0.12f;
};
