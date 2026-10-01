#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "Input.h"
#include <vector>
#include <wrl.h>
#include "Logger.h"
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

std::unique_ptr <Input> Input::instance = nullptr;

// 共通の定数
const float STICK_MAX = 32768.0f;
const float DEAD_ZONE = 0.1f; // 10%以下の傾きは無視する

// DirectInputの各装置を準備し、失敗時もReleaseで空の装置を参照しないようにします。
void Input::Initialize(WinApp* winApp)
{

	//借りてきたWinAppのインスタンスを記憶
	//Inputのメンバ変数とInitializeの仮引数
	this->winApp_ = winApp;

	HRESULT result;

	//DirectInputのインスタンス生成
	result = DirectInput8Create(winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	if (FAILED(result)) {
		Logger::Log("Input: DirectInput initialization failed; mouse uses Windows fallback.\n");
		return;
	}
	//キーボードデバイス生成
	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	if (SUCCEEDED(result)) {
		// Windowsキーを妨げない協調レベルで、キーボードを使える状態にします。
		result = keyboard->SetDataFormat(&c_dfDIKeyboard);
		if (SUCCEEDED(result)) {
			result = keyboard->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		}
	}
	if (FAILED(result)) {
		Logger::Log("Input: DirectInput keyboard initialization failed.\n");
		keyboard.Reset();
	}

	// マウスデバイス生成
	result = directInput->CreateDevice(GUID_SysMouse, &mouse, nullptr);
	if (SUCCEEDED(result)) {
		// マウスの初期化に失敗しても、更新時のWindows入力で操作を続けられるようにする。
		result = mouse->SetDataFormat(&c_dfDIMouse);
		if (SUCCEEDED(result)) {
			result = mouse->SetCooperativeLevel(
				winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		}
		if (SUCCEEDED(result)) {
			result = mouse->Acquire();
		}
	}
	if (FAILED(result) && result != DIERR_OTHERAPPHASPRIO) {
		Logger::Log("Input: DirectInput mouse initialization failed; Windows mouse fallback is available.\n");
	}

	SearchGamepads();

	//gamepadsの中に要素があればコントローラー接続と判断
	if (!gamepads.empty())
	{
		currentDevice_ = InputDevice::Gamepad;
	} else
	{
		currentDevice_ = InputDevice::Keyboard;
	}
}

// 前回の状態を保存してから各装置を更新し、Trigger判定と操作機器の判定順を保ちます。
void Input::Update()
{
	//前回のキー入力を保存
	memcpy(keyPre, key, sizeof(key));
	mouseStatePre = mouseState;
	UpdateKeyboard();
	UpdateMouse();
	UpdateGamepads();
	UpdateCurrentDevice();
}

// キーボード状態を取得し、フォーカス喪失時に古い押下状態を残さないようにします。
void Input::UpdateKeyboard()
{
	//キーボード情報の取得開始
	if (keyboard) {
		keyboard->Acquire();
		// フォーカスを失った時は古い押下状態を残さない。
		if (FAILED(keyboard->GetDeviceState(sizeof(key), key))) {
			ZeroMemory(key, sizeof(key));
			// Releaseで操作不能になった時、前面表示中の取得失敗だけを診断できます。
			if (GetForegroundWindow() == winApp_->GetHwnd() && !keyboardUnavailableLogged_) {
				Logger::Log("Input: DirectInput keyboard state unavailable; keyboard input cleared.");
				keyboardUnavailableLogged_ = true;
			}
		} else {
			keyboardUnavailableLogged_ = false;
		}
	}
}

// マウスの位置・ボタンを取得し、DirectInputが使えない時だけWindows入力で補います。
void Input::UpdateMouse()
{
	// マウススクリーン座標取得
	POINT pos{ mouseScreenX, mouseScreenY };
	if (GetCursorPos(&pos)) {
		ScreenToClient(winApp_->GetHwnd(), &pos);
	}

	mouseScreenX = pos.x;
	mouseScreenY = pos.y;

	// マウスボタン
   // 現在の状態をクリアしてから取得
	ZeroMemory(&mouseState, sizeof(mouseState));

	HRESULT hr = mouse ? mouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState) : E_FAIL;
	if (FAILED(hr) && mouse) {
		// フォーカス復帰後の再取得結果も確認する。
		mouse->Acquire();
		hr = mouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState);
	}
	if (FAILED(hr)) {
		// DirectInputが使えない間だけWindows入力に切り替え、Releaseの操作を失わない。
		if (!mouseFallbackLogged_) {
			Logger::Log("Input: DirectInput mouse state unavailable; using Windows mouse input.\n");
			mouseFallbackLogged_ = true;
		}
		if (GetForegroundWindow() == winApp_->GetHwnd()) {
			if (hasPreviousMousePosition_) {
				mouseState.lX = pos.x - previousMousePosition_.x;
				mouseState.lY = pos.y - previousMousePosition_.y;
			}
			const int buttons[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
			for (int button = 0; button < 3; ++button) {
				mouseState.rgbButtons[button] = (GetAsyncKeyState(buttons[button]) & 0x8000) ? 0x80 : 0;
			}
		}
	} else {
		mouseFallbackLogged_ = false;
	}
	previousMousePosition_ = pos;
	hasPreviousMousePosition_ = true;
}

// 接続済みゲームパッドを更新し、未接続なら一定間隔で再検索します。
void Input::UpdateGamepads()
{
	if (gamepads.empty()) {
		reSearchTimer_++;
		if (reSearchTimer_ >= 60) { // 60フレーム（約1秒）に1回実行
			reSearchTimer_ = 0;
			SearchGamepads(); // 再検索！

			// もしこのタイミングで見つかったら、デバイス状態をゲームパッドにする
			if (!gamepads.empty()) {
				currentDevice_ = InputDevice::Gamepad;
			}
		}
	}
	padStatesPre = padStates;
	// 各パッドの現在値を読み、Trigger判定用に前フレームの値を残します。
	for (size_t i = 0; i < gamepads.size(); i++) {
		gamepads[i]->Poll();

		// 現在の状態を取得
		HRESULT hr = gamepads[i]->GetDeviceState(sizeof(DIJOYSTATE), &padStates[i]);

		// もし取得に失敗（ウィンドウからフォーカスが外れた等）したら再取得を試みる
		if (FAILED(hr)) {
			gamepads[i]->Acquire();
		}
	}
}

// キーボード・マウス・パッドの順に判定し、操作案内に使う機器を決めます。
void Input::UpdateCurrentDevice()
{
	//キーボードの何かしら押されたかチェック
	for (int i = 0; i < 256; i++)
	{
		if (key[i])//どこかのキーが押されていたら
		{
			currentDevice_ = InputDevice::Keyboard;
			break;//キーボードに確定したのでループを抜ける
		}
	}

	//マウスの何かしら押されたかチェック
	if (mouseState.lX != 0 || mouseState.lY != 0 || mouseState.lZ != 0 ||
		mouseState.rgbButtons[0] || mouseState.rgbButtons[1] ||
		mouseState.rgbButtons[2] || mouseState.rgbButtons[3])
	{
		currentDevice_ = InputDevice::Keyboard; // マウスを触った場合もキーボードUIにする
	}

	//コントローラーの何かしらのボタンまたはスティックが反応したらチェック
	for (size_t i = 0; i < gamepads.size(); i++)
	{//ボタンチェック
		for (int b = 0; b < 32; b++)
		{
			if (padStates[i].rgbButtons[b])
			{
				currentDevice_ = InputDevice::Gamepad;
				break;
			}
		}
		//スティックチェック
		if (GetPadLeftAxisX(static_cast<int>(i)) != 0.0f ||
			GetPadLeftAxisY(static_cast<int>(i)) != 0.0f ||
			GetPadRightAxisX(static_cast<int>(i)) != 0.0f ||
			GetPadRightAxisY(static_cast<int>(i)) != 0.0f)
		{
			currentDevice_ = InputDevice::Gamepad;
		}
	}
}

// ゲームパッドを列挙し、現在接続されている装置とボタン状態だけを保持します。
void Input::SearchGamepads() {
	// 念のためリストを一度クリアする
	gamepads.clear();
	padStates.clear();
	padStatesPre.clear();
	if (!directInput) {
		return;
	}

	// 接続されているパッドを探す（Initialize内にあった処理の引っ越し）
	HRESULT result = directInput->EnumDevices(
		DI8DEVCLASS_GAMECTRL,
		[](const DIDEVICEINSTANCE* pdidInstance, VOID* pContext) -> BOOL {
			auto self = static_cast<Input*>(pContext);
			ComPtr<IDirectInputDevice8> newPad;

			if (FAILED(self->directInput->CreateDevice(pdidInstance->guidInstance, &newPad, nullptr))) {
				return DIENUM_CONTINUE;
			}

			newPad->SetDataFormat(&c_dfDIJoystick);
			newPad->SetCooperativeLevel(self->winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

			DIPROPRANGE diprg;
			diprg.diph.dwSize = sizeof(DIPROPRANGE);
			diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
			diprg.diph.dwHow = DIPH_DEVICE;
			diprg.diph.dwObj = 0;
			diprg.lMin = -32768; // 最小値
			diprg.lMax = 32768;  // 最大値
			newPad->SetProperty(DIPROP_RANGE, &diprg.diph);

			self->gamepads.push_back(newPad);
			self->padStates.emplace_back();
			self->padStatesPre.emplace_back();

			return DIENUM_CONTINUE;
		},
		this, DIEDFL_ATTACHEDONLY);
}


Input* Input::GetInstance() {
	if (instance == nullptr) {
		instance = std::make_unique <Input>();
	}
	return instance.get();
}

//プッシュ入力
bool Input::PushKey(BYTE keyNumber){
	//指定キーを押していればtrueを返す
	if (key[keyNumber]) {
		return true;
	}
	//そうでなければfalseを返す
	return false;
}
//トリガー入力
bool Input::TriggerKey(BYTE keyNumber){
	if (key[keyNumber] && !keyPre[keyNumber]){
		return true;
	}
	return false;
}
// ゲームパッドのボタンが押された瞬間かどうか
bool Input::TriggerPadButton(int padIndex, int button) {
	// 指定されたインデックスが範囲外なら拒否
	if (padIndex < 0 || padIndex >= static_cast<int>(padStates.size()) || button < 0 || button >= 32) return false;

	// 「現在のフレームで押されていて」かつ「前のフレームでは押されていなかった」なら true
	if (padStates[padIndex].rgbButtons[button] && !padStatesPre[padIndex].rgbButtons[button]) {
		return true;
	}

	return false;
}

// マウスボタンが押されたかどうか
bool Input::IsMouseButtonPressed(int button) {
	if (button < 0 || button >= 4) return false;
	if (mouseState.rgbButtons[button] & 0x80) {
		return true;
	}
	return false;
}

bool Input::TriggerMouseButton(int button) {
	if (button < 0 || button >= 4) return false;
	return (mouseState.rgbButtons[button] & 0x80) && !(mouseStatePre.rgbButtons[button] & 0x80);
}

// ゲームパッドのボタンが押されたかどうか
bool Input::IsPadButtonPressed(int padIndex, int button) {
	if (padIndex < 0 || padIndex >= static_cast<int>(padStates.size()) || button < 0 || button >= 32) return false;

	if (padStates[padIndex].rgbButtons[button]) {
		return true;
	}

	return false;
}

// ゲームパッドの軸の値を取得
float Input::GetPadLeftAxisX(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates.size())) {
		float val = static_cast<float>(padStates[padIndex].lX) / STICK_MAX;
		return (abs(val) < DEAD_ZONE) ? 0.0f : val;
	}
	return 0.0f;
}
float Input::GetPadLeftAxisY(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates.size())) {
		// Y軸は上がマイナスで返ってくることが多いため、ゲームに合わせて反転させることもある
		float val = static_cast<float>(padStates[padIndex].lY) / STICK_MAX;
		return (abs(val) < DEAD_ZONE) ? 0.0f : val;
	}
	return 0.0f;
}

float Input::GetPadRightAxisX(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates.size())) {
		float val = static_cast<float>(padStates[padIndex].lRx) / STICK_MAX;
		return (abs(val) < DEAD_ZONE) ? 0.0f : val;
	}
	return 0.0f;
}

float Input::GetPadRightAxisY(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates.size())) {
		float val = static_cast<float>(padStates[padIndex].lRy) / STICK_MAX;
		return (abs(val) < DEAD_ZONE) ? 0.0f : val;
	}
	return 0.0f;
}
