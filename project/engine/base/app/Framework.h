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

	// 共通資源を依存順に作り、失敗した資源の後続を始めずScene開始前に終了を要求します。
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

	// 初期化→更新・描画→終了の順を固定し、派生Gameの処理を呼ぶTemplate Methodです。
	int Run();

protected:
	// Factoryの所有をFrameworkへ移し、SceneManagerには生存中の参照だけを渡します。
	void SetSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory);
	// Game側のテスト終了時も、終了要求と結果を同時に記録します。
	void RequestEnd(int exitCode = 0);

private:
	bool endRequest_ = false;
	int exitCode_ = 0;
	// 初期化途中で終了する時、まだ開始していないAudioとImGuiを解放しません。
	bool isAudioInitialized_ = false;
	bool isImGuiInitialized_ = false;

	// FactoryとWindowはFrameworkが所有し、Managerの共有本体は借りて使います。
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;
	std::unique_ptr<Engine::WinApp> winApp_;
	Engine::DirectXCommon* dxCommon_ = nullptr;
	Engine::SrvManager* srvManager_ = nullptr;
	Engine::Input* input_ = nullptr;
	ImGuiManager* imGuiManager_ = nullptr;

};

