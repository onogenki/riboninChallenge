#pragma once
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Input.h"
#include "ImGuiManager.h"
#include "BaseScene.h"
#include "SceneManager.h"
#include <xaudio2.h>
#include <wrl.h>    

class AbstractSceneFactory;

//ゲーム全体
class Framework
{
public:

	//継承先で中身を作る関数
	virtual void Initialize();
	virtual void Finalize();
	virtual void Update();
	virtual void Draw() = 0;//純粋仮想関数

	virtual bool IsEndRequest() { return endRequest_; }

	virtual ~Framework() = default;//仮想デストラクタ

	//実行
	void Run();

protected:// 派生クラス（Game）からアクセス可能にする
	bool endRequest_ = false;

	// どのゲームでも使う汎用インスタンス
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;
	std::unique_ptr<WinApp> winApp_;
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Input* input_ = nullptr;
	ImGuiManager* imGuiManager_ = nullptr;
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

};

