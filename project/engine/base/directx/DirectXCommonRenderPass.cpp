#include "DirectXCommon.h"

#include <cassert>

// Scene、PostEffect、SwapChainを切り替えるフレーム描画処理をまとめる。
// Device初期化やTexture Resource作成から分離し、描画順を追いやすくする。
void DirectXCommon::PreDraw(const Vector4* clearColorOverride)
{
	if (isDepthStencilShaderResource_)
	{
		D3D12_RESOURCE_BARRIER depthStencilBarrier{};
		depthStencilBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		depthStencilBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		depthStencilBarrier.Transition.pResource = depthStencilResource.Get();
		depthStencilBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		depthStencilBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		depthStencilBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &depthStencilBarrier);
		isDepthStencilShaderResource_ = false;
	}

	// 2フレーム目以降は、読み取り状態からSceneを描ける状態へ戻す
	if (isRenderTextureShaderResource_)
	{

	//TransitionBarrierの設定
	D3D12_RESOURCE_BARRIER barrier{};
	//今回のバリアはTransition
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//Noneにしておく
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//バリアを張る対象のリソース。現在のバックバッファに対して行う
	barrier.Transition.pResource = renderTextureResource_.Get();
	//遷移前(現在)のResourceState
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	//遷移後のResourceState
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	//TransitionBarrierを張る
	commandList_->ResourceBarrier(1, &barrier);
		isRenderTextureShaderResource_ = false;
	}

	// Sceneの描画先をRenderTextureにする。Sceneでは深度も使用する
	commandList_->OMSetRenderTargets(1, &renderTextureRtvHandle_, false, &dsvHandle_);

	// Loadingだけは白を使い、それ以外は従来の消去色でRenderTextureをクリアします。
	const Vector4& sceneClearColor = clearColorOverride ? *clearColorOverride : renderTextureClearColor_;
	const float clearColor[] = {
		sceneClearColor.x,
		sceneClearColor.y,
		sceneClearColor.z,
		sceneClearColor.w
	};
	commandList_->ClearRenderTargetView(renderTextureRtvHandle_, clearColor, 0, nullptr);

	//指定した深度で画面全体をクリアする
	commandList_->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	commandList_->RSSetViewports(1, &viewport_);

	commandList_->RSSetScissorRects(1, &scissorRect_);//scirssorを設定

}

void DirectXCommon::PreDrawForPostEffectTexture()
{
	if (!isRenderTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER renderTextureBarrier{};
		renderTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		renderTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		renderTextureBarrier.Transition.pResource = renderTextureResource_.Get();
		renderTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		renderTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		renderTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &renderTextureBarrier);
		isRenderTextureShaderResource_ = true;
	}

	if (isPostEffectTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER postEffectTextureBarrier{};
		postEffectTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		postEffectTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		postEffectTextureBarrier.Transition.pResource = postEffectTextureResource_.Get();
		postEffectTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		postEffectTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		postEffectTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &postEffectTextureBarrier);
		isPostEffectTextureShaderResource_ = false;
	}

	commandList_->OMSetRenderTargets(1, &postEffectTextureRtvHandle_, false, nullptr);

	const float clearColor[] = {
		renderTextureClearColor_.x,
		renderTextureClearColor_.y,
		renderTextureClearColor_.z,
		renderTextureClearColor_.w
	};
	commandList_->ClearRenderTargetView(postEffectTextureRtvHandle_, clearColor, 0, nullptr);

	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::PreDrawForDepthBasedOutlineTexture()
{
	PreDrawForPostEffectTexture();

	if (!isDepthStencilShaderResource_)
	{
		D3D12_RESOURCE_BARRIER depthStencilBarrier{};
		depthStencilBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		depthStencilBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		depthStencilBarrier.Transition.pResource = depthStencilResource.Get();
		depthStencilBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		depthStencilBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		depthStencilBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &depthStencilBarrier);
		isDepthStencilShaderResource_ = true;
	}
}

void DirectXCommon::PreDrawForGaussianHorizontalTexture()
{
	if (!isRenderTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER renderTextureBarrier{};
		renderTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		renderTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		renderTextureBarrier.Transition.pResource = renderTextureResource_.Get();
		renderTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		renderTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		renderTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &renderTextureBarrier);
		isRenderTextureShaderResource_ = true;
	}

	if (isGaussianBlurTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER gaussianBlurTextureBarrier{};
		gaussianBlurTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		gaussianBlurTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		gaussianBlurTextureBarrier.Transition.pResource = gaussianBlurTextureResource_.Get();
		gaussianBlurTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		gaussianBlurTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gaussianBlurTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &gaussianBlurTextureBarrier);
		isGaussianBlurTextureShaderResource_ = false;
	}

	commandList_->OMSetRenderTargets(1, &gaussianBlurTextureRtvHandle_, false, nullptr);

	const float clearColor[] = {
		renderTextureClearColor_.x,
		renderTextureClearColor_.y,
		renderTextureClearColor_.z,
		renderTextureClearColor_.w
	};
	commandList_->ClearRenderTargetView(gaussianBlurTextureRtvHandle_, clearColor, 0, nullptr);

	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::PreDrawForGaussianVerticalTexture()
{
	if (!isGaussianBlurTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER gaussianBlurTextureBarrier{};
		gaussianBlurTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		gaussianBlurTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		gaussianBlurTextureBarrier.Transition.pResource = gaussianBlurTextureResource_.Get();
		gaussianBlurTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		gaussianBlurTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		gaussianBlurTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &gaussianBlurTextureBarrier);
		isGaussianBlurTextureShaderResource_ = true;
	}

	if (isPostEffectTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER postEffectTextureBarrier{};
		postEffectTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		postEffectTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		postEffectTextureBarrier.Transition.pResource = postEffectTextureResource_.Get();
		postEffectTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		postEffectTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		postEffectTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &postEffectTextureBarrier);
		isPostEffectTextureShaderResource_ = false;
	}

	commandList_->OMSetRenderTargets(1, &postEffectTextureRtvHandle_, false, nullptr);

	const float clearColor[] = {
		renderTextureClearColor_.x,
		renderTextureClearColor_.y,
		renderTextureClearColor_.z,
		renderTextureClearColor_.w
	};
	commandList_->ClearRenderTargetView(postEffectTextureRtvHandle_, clearColor, 0, nullptr);

	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::PreDrawForSwapChain(bool usePostEffectTexture)
{
	// Sceneの描画結果を後でSRVから読めるよう、RenderTextureを書き込み状態から読み取り状態へ変更する
	if (!isRenderTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER renderTextureBarrier{};
		renderTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		renderTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		renderTextureBarrier.Transition.pResource = renderTextureResource_.Get();
		renderTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		renderTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		renderTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &renderTextureBarrier);
		isRenderTextureShaderResource_ = true;
	}

	if (usePostEffectTexture && !isPostEffectTextureShaderResource_)
	{
		D3D12_RESOURCE_BARRIER postEffectTextureBarrier{};
		postEffectTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		postEffectTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		postEffectTextureBarrier.Transition.pResource = postEffectTextureResource_.Get();
		postEffectTextureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		postEffectTextureBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		postEffectTextureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &postEffectTextureBarrier);
		isPostEffectTextureShaderResource_ = true;
	}

	// ImGuiを表示するため、SwapChainを描画可能な状態へ変更する
	const UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
	D3D12_RESOURCE_BARRIER swapChainBarrier{};
	swapChainBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	swapChainBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	swapChainBarrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	swapChainBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	swapChainBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	swapChainBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList_->ResourceBarrier(1, &swapChainBarrier);

	// SwapChainにはImGuiだけを描画する。Scene用の深度は不要なのでDSVはnullptrにする
	commandList_->OMSetRenderTargets(1, &rtvHandle_[backBufferIndex], false, nullptr);

	// ImGuiの背景となるSwapChainを青色でクリアする
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	commandList_->ClearRenderTargetView(rtvHandle_[backBufferIndex], clearColor, 0, nullptr);

	commandList_->RSSetViewports(1, &viewport_);
	commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::PostDraw()
{
	//バックバッファの番号取得
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

	//TransitionBarrierの設定
	D3D12_RESOURCE_BARRIER barrier{};
	//今回のバリアはTransition
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//Noneにしておく
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//バリアを張る対象のリソース。現在のバックバッファに対して行う
	barrier.Transition.pResource = swapChainResources[backBufferIndex].Get();
	//遷移前(現在)のResourceState
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//遷移後のResourceState
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	
	//TransitionBarrierを張る
	commandList_->ResourceBarrier(1, &barrier);

	hr = GetCommandList()->Close();
	assert(SUCCEEDED(hr));

	//GPUにコマンドリストの実行を行わせる
	Microsoft::WRL::ComPtr < ID3D12CommandList> commandLists[] = { GetCommandList() };
	commandQueue->ExecuteCommandLists(1, commandLists->GetAddressOf());
	//GPUとOSに画面の交渉を行うように通知する
	swapChain->Present(1, 0);

	//Fenceの値を更新
	const UINT64 signalValue = ++fenceVal;
	//GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
	commandQueue->Signal(fence.Get(), signalValue);
	frameFenceValues_[frameIndex_] = signalValue;
	//Fenceの値が指定したSignal値にたどり着いているか確認する
	//GetCompletedValueの初期値はFence作成時に渡した初期値
	frameIndex_ = swapChain->GetCurrentBackBufferIndex();
	if (frameFenceValues_[frameIndex_] != 0 && fence->GetCompletedValue() < frameFenceValues_[frameIndex_])
	{
		//指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
		fence->SetEventOnCompletion(frameFenceValues_[frameIndex_], fenceEvent_);
		//イベント待つ
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

	//次のフレーム用のコマンドリストを準備
	hr = commandAllocators_[frameIndex_]->Reset();
	assert(SUCCEEDED(hr));
	hr = GetCommandList()->Reset(commandAllocators_[frameIndex_].Get(), nullptr);
	assert(SUCCEEDED(hr));
	// 次のUpdate/Drawは新しいフレームとして扱い、描画回ごとのUpload領域を先頭から使います。
	++frameSerial_;

	//FPS固定
	UpdateFixFPS();

}


