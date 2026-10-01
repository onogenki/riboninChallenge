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

	// マウス
	LONG GetMouseX() const { return mouseState.lX; }
	LONG GetMouseY() const { return mouseState.lY; }
	LONG GetMouseWheel() const { return mouseState.lZ; }
	Vector2 GetMouseScreen() const {
		return Vector2{
static_cast<float>(mouseScreenX),
static_cast<float>(mouseScreenY)
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

	//現在の操作機種を取得する関数
	InputDevice GetCurrentDevice() const { return currentDevice_; }

private:
	// シングルトンインスタンス
	static std::unique_ptr <Input> instance;

	//DirectInputのインスタンス
	ComPtr<IDirectInput8>directInput = nullptr;

	//キーボードのデバイス
	ComPtr<IDirectInputDevice8>keyboard = nullptr;
	//今回の全キーの状態
	BYTE key[256] = {};
	//前回の全キーの状態
	BYTE keyPre[256] = {};
	// 前面表示中にキーボード取得が失敗した時だけ、一度ログへ知らせます。
	bool keyboardUnavailableLogged_ = false;

	// マウス
	ComPtr<IDirectInputDevice8> mouse = nullptr;
	DIMOUSESTATE mouseStatePre{};
	DIMOUSESTATE mouseState{}; // マウスの状態
	int mouseScreenX = 0; // マウスのスクリーン座標X
	int mouseScreenY = 0; // マウスのスクリーン座標Y
	POINT previousMousePosition_{};
	bool hasPreviousMousePosition_ = false;
	bool mouseFallbackLogged_ = false;

	// ゲームパッド
	std::vector<ComPtr<IDirectInputDevice8>> gamepads;
	ComPtr<IDirectInputDevice8> newGamepad;
	std::vector<DIJOYSTATE> padStates;
	std::vector<DIJOYSTATE> padStatesPre;

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

