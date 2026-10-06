#pragma once
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Input.h"
#include "ImGuiManager.h"
#include "BaseScene.h"
#include "SceneManager.h"

class AbstractSceneFactory;

// Window・描画・入力・Audioなどの共通資源を初期化し、ゲームループを実行します。
// Sceneの内容と描画対象は派生先のGameと各Sceneへ任せます。
class Framework
{
public:

	// 共通資源を依存順に作り、派生先のGameがSceneを追加できる状態にします。
	virtual void Initialize();
	// Sceneを先に終了し、描画・入力・Audioで使った共通資源を解放します。
	virtual void Finalize();
	// 入力・Audio・Resourceを更新してから、現在Sceneの更新へ進みます。
	virtual void Update();
	// 派生先が現在Sceneの描画を実装します。
	virtual void Draw() = 0;

	// Windowなどから終了要求が届いた時にtrueを返します。
	virtual bool IsEndRequest() { return endRequest_; }

	// 派生先のGameを基底ポインタから破棄できるようにします。
	virtual ~Framework() = default;

	// Initialize → Update/Drawの繰り返し → Finalizeの順に実行します。
	void Run();

protected:
	// 派生先のGameが終了を予約するための状態です。
	bool endRequest_ = false;

	// FactoryとWindowはFrameworkが所有し、Managerの共有本体は借りて使います。
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;
	std::unique_ptr<WinApp> winApp_;
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Input* input_ = nullptr;
	ImGuiManager* imGuiManager_ = nullptr;

};

