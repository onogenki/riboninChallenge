#pragma once
#include<wrl.h>
#include <vector>
#include<memory>
#define DIRECTINPUT_VERSION 0x0800  //DirectInputのバージョン指定
#include<dinput.h>
#include"WinApp.h"
#include"Vector2.h"

//操作する機種の種類を表す
enum class InputDevice
{
	Keyboard,
	Gamepad
};

// キーボード・マウス・ゲームパッドの状態をフレームごとに取得します。
class Input
{
public:

	// シングルトンインスタンスの取得
	static Input* GetInstance();

	//namespace省略
	template<class T>using ComPtr = Microsoft::WRL::ComPtr <T>;

	void Initialize(WinApp* winApp);

	void InitializeDXGIDevice();

	void Update();

	bool PushKey(BYTE keyNumber);
	bool TriggerKey(BYTE keyNumber);
	// 除外キー以外の新しい押下が一つでもあるか、ポーズ解除用に調べます。
	bool TriggerAnyKeyExcept(BYTE excludedKey) const;

	// マウス
	// フレームで取得した移動量を返し、Camera操作とUI判定で同じ値を使います。
	LONG GetMouseX() const { return mouseState_.lX; }
	LONG GetMouseY() const { return mouseState_.lY; }
	LONG GetMouseWheel() const { return mouseState_.lZ; }
	// Windowsのクライアント座標を返し、Game View内の判定に使います。
	Vector2 GetMouseScreen() const {
		return Vector2{
static_cast<float>(mouseScreenX_),
static_cast<float>(mouseScreenY_)
		};
	}
	bool IsMouseButtonPressed(int button);
	bool TriggerMouseButton(int button);

	// ゲームパッド
	bool IsPadButtonPressed(int padIndex, int button);//プッシュ
	bool TriggerPadButton(int padIndex, int button);//押された瞬間
	float GetPadLeftAxisX(int padIndex);
	float GetPadLeftAxisY(int padIndex);
	float GetPadRightAxisX(int padIndex);
	float GetPadRightAxisY(int padIndex);

	// 最後に操作された機器を返し、画面の操作案内を切り替えます。
	InputDevice GetCurrentDevice() const { return currentDevice_; }

private:
	// シングルトンインスタンス
	static std::unique_ptr <Input> instance_;

	//DirectInputのインスタンス
	ComPtr<IDirectInput8>directInput_ = nullptr;

	//キーボードのデバイス
	ComPtr<IDirectInputDevice8>keyboard_ = nullptr;
	//今回の全キーの状態
	BYTE key_[256] = {};
	//前回の全キーの状態
	BYTE keyPre_[256] = {};
	// 前面表示中にキーボード取得が失敗した時だけ、一度ログへ知らせます。
	bool keyboardUnavailableLogged_ = false;

	// マウス
	ComPtr<IDirectInputDevice8> mouse_ = nullptr;
	DIMOUSESTATE mouseStatePre_{};
	DIMOUSESTATE mouseState_{}; // マウスの状態
	int mouseScreenX_ = 0; // マウスのスクリーン座標X
	int mouseScreenY_ = 0; // マウスのスクリーン座標Y
	POINT previousMousePosition_{};
	bool hasPreviousMousePosition_ = false;
	bool mouseFallbackLogged_ = false;

	// ゲームパッド
	std::vector<ComPtr<IDirectInputDevice8>> gamepads_;
	std::vector<DIJOYSTATE> padStates_;
	std::vector<DIJOYSTATE> padStatesPre_;

	//現在の操作機種を記憶する変数を追加(初期値はキーボード)
	InputDevice currentDevice_ = InputDevice::Keyboard;

	void UpdateKeyboard();
	void UpdateMouse();
	void UpdateGamepads();
	void UpdateCurrentDevice();
	void SearchGamepads();   // コントローラーを検索する関数を独立させる
	int reSearchTimer_ = 0;  // 再検索までのフレームを数えるタイマー

	//WindowsAPI
	WinApp* winApp_ = nullptr;
};

