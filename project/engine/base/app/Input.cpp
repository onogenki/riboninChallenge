#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "Input.h"
#include <vector>
#include <wrl.h>
#include <cmath>
#include "Logger.h"
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

namespace Engine
{
std::unique_ptr <Input> Input::instance_ = nullptr;

namespace {
constexpr float kStickMax = 32768.0f;
constexpr float kDeadZone = 0.1f;

// DirectInputで設定した軸範囲を-1～1へ直し、10%未満の揺れを入力にしません。
float NormalizePadAxis(LONG rawAxis)
{
	const float value = static_cast<float>(rawAxis) / kStickMax;
	return (std::abs(value) < kDeadZone) ? 0.0f : value;
}
}

// DirectInputの各装置を準備し、失敗時もReleaseで空の装置を参照しないようにします。
void Input::Initialize(WinApp* winApp)
{

	//借りてきたWinAppのインスタンスを記憶
	//Inputのメンバ変数とInitializeの仮引数
	this->winApp_ = winApp;

	HRESULT result;

	//DirectInputのインスタンス生成
	result = DirectInput8Create(winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput_, nullptr);
	if (FAILED(result)) {
		Logger::Log("Input: DirectInput initialization failed; mouse uses Windows fallback.\n");
		return;
	}
	//キーボードデバイス生成
	result = directInput_->CreateDevice(GUID_SysKeyboard, &keyboard_, NULL);
	if (SUCCEEDED(result)) {
		// Windowsキーを妨げない協調レベルで、キーボードを使える状態にします。
		result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
		if (SUCCEEDED(result)) {
			result = keyboard_->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		}
	}
	if (FAILED(result)) {
		Logger::Log("Input: DirectInput keyboard initialization failed.\n");
		keyboard_.Reset();
	}

	// マウスデバイス生成
	result = directInput_->CreateDevice(GUID_SysMouse, &mouse_, nullptr);
	if (SUCCEEDED(result)) {
		// マウスの初期化に失敗しても、更新時のWindows入力で操作を続けられるようにする。
		result = mouse_->SetDataFormat(&c_dfDIMouse);
		if (SUCCEEDED(result)) {
			result = mouse_->SetCooperativeLevel(
				winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		}
		if (SUCCEEDED(result)) {
			result = mouse_->Acquire();
		}
	}
	if (FAILED(result) && result != DIERR_OTHERAPPHASPRIO) {
		Logger::Log("Input: DirectInput mouse initialization failed; Windows mouse fallback is available.\n");
	}

	SearchGamepads();

	//gamepadsの中に要素があればコントローラー接続と判断
	if (!gamepads_.empty())
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
	memcpy(keyPre_, key_, sizeof(key_));
	mouseStatePre_ = mouseState_;
	UpdateKeyboard();
	UpdateMouse();
	UpdateGamepads();
	UpdateCurrentDevice();
}

// キーボード状態を取得し、フォーカス喪失時に古い押下状態を残さないようにします。
void Input::UpdateKeyboard()
{
	//キーボード情報の取得開始
	if (keyboard_) {
		keyboard_->Acquire();
		// フォーカスを失った時は古い押下状態を残さない。
		if (FAILED(keyboard_->GetDeviceState(sizeof(key_), key_))) {
			ZeroMemory(key_, sizeof(key_));
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
	POINT pos{ mouseScreenX_, mouseScreenY_ };
	if (GetCursorPos(&pos)) {
		ScreenToClient(winApp_->GetHwnd(), &pos);
	}

	mouseScreenX_ = pos.x;
	mouseScreenY_ = pos.y;

	// マウスボタン
   // 現在の状態をクリアしてから取得
	ZeroMemory(&mouseState_, sizeof(mouseState_));

	HRESULT hr = mouse_ ? mouse_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState_) : E_FAIL;
	if (FAILED(hr) && mouse_) {
		// フォーカス復帰後の再取得結果も確認する。
		mouse_->Acquire();
		hr = mouse_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState_);
	}
	if (FAILED(hr)) {
		// DirectInputが使えない間だけWindows入力に切り替え、Releaseの操作を失わない。
		if (!mouseFallbackLogged_) {
			Logger::Log("Input: DirectInput mouse state unavailable; using Windows mouse input.\n");
			mouseFallbackLogged_ = true;
		}
		if (GetForegroundWindow() == winApp_->GetHwnd()) {
			if (hasPreviousMousePosition_) {
				mouseState_.lX = pos.x - previousMousePosition_.x;
				mouseState_.lY = pos.y - previousMousePosition_.y;
			}
			const int buttons[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
			for (int button = 0; button < 3; ++button) {
				mouseState_.rgbButtons[button] = (GetAsyncKeyState(buttons[button]) & 0x8000) ? 0x80 : 0;
			}
		}
	} else {
		mouseFallbackLogged_ = false;
	}
	previousMousePosition_ = pos;
	hasPreviousMousePosition_ = true;
}

// 未接続なら再検索し、取得失敗時は古い押下状態を残さず再取得します。
void Input::UpdateGamepads()
{
	if (gamepads_.empty()) {
		reSearchTimer_++;
		if (reSearchTimer_ >= 60) { // 60フレーム（約1秒）に1回実行
			reSearchTimer_ = 0;
			SearchGamepads(); // 再検索！

			// もしこのタイミングで見つかったら、デバイス状態をゲームパッドにする
			if (!gamepads_.empty()) {
				currentDevice_ = InputDevice::Gamepad;
			}
		}
	}
	padStatesPre_ = padStates_;
	// 各パッドの現在値を読み、Trigger判定用に前フレームの値を残します。
	for (size_t i = 0; i < gamepads_.size(); i++) {
		gamepads_[i]->Poll();

		// 読めないフレームでは前回の押下を使わないよう、結果は新しい状態へ受け取ります。
		DIJOYSTATE currentState{};
		HRESULT hr = gamepads_[i]->GetDeviceState(sizeof(DIJOYSTATE), &currentState);
		if (FAILED(hr)) {
			gamepads_[i]->Acquire();
			currentState = {};
			hr = gamepads_[i]->GetDeviceState(sizeof(DIJOYSTATE), &currentState);
		}
		padStates_[i] = SUCCEEDED(hr) ? currentState : DIJOYSTATE{};
	}
}

// キーボード・マウス・パッドの順に判定し、操作案内に使う機器を決めます。
void Input::UpdateCurrentDevice()
{
	//キーボードの何かしら押されたかチェック
	for (int i = 0; i < 256; i++)
	{
		if (key_[i])//どこかのキーが押されていたら
		{
			currentDevice_ = InputDevice::Keyboard;
			break;//キーボードに確定したのでループを抜ける
		}
	}

	//マウスの何かしら押されたかチェック
	if (mouseState_.lX != 0 || mouseState_.lY != 0 || mouseState_.lZ != 0 ||
		mouseState_.rgbButtons[0] || mouseState_.rgbButtons[1] ||
		mouseState_.rgbButtons[2] || mouseState_.rgbButtons[3])
	{
		currentDevice_ = InputDevice::Keyboard; // マウスを触った場合もキーボードUIにする
	}

	//コントローラーの何かしらのボタンまたはスティックが反応したらチェック
	for (size_t i = 0; i < gamepads_.size(); i++)
	{//ボタンチェック
		for (int b = 0; b < 32; b++)
		{
			if (padStates_[i].rgbButtons[b])
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
	gamepads_.clear();
	padStates_.clear();
	padStatesPre_.clear();
	if (!directInput_) {
		return;
	}

	// 接続されているパッドを探す（Initialize内にあった処理の引っ越し）
	HRESULT result = directInput_->EnumDevices(
		DI8DEVCLASS_GAMECTRL,
		[](const DIDEVICEINSTANCE* pdidInstance, VOID* pContext) -> BOOL {
			auto self = static_cast<Input*>(pContext);
			ComPtr<IDirectInputDevice8> newPad;

			if (FAILED(self->directInput_->CreateDevice(pdidInstance->guidInstance, &newPad, nullptr))) {
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

			self->gamepads_.push_back(newPad);
			self->padStates_.emplace_back();
			self->padStatesPre_.emplace_back();

			return DIENUM_CONTINUE;
		},
		this, DIEDFL_ATTACHEDONLY);
}


// 入力状態をフレーム間で共有するため、同じInput本体を返します。
Input* Input::GetInstance() {
	if (instance_ == nullptr) {
		instance_ = std::make_unique <Input>();
	}
	return instance_.get();
}

// 現在フレームの指定キーを読み、押し続け中の操作にも使えるようにします。
bool Input::PushKey(BYTE keyNumber){
	//指定キーを押していればtrueを返す
	if (key_[keyNumber]) {
		return true;
	}
	//そうでなければfalseを返す
	return false;
}
// 前フレームとの差を調べ、一回だけ反応させたい操作に使います。
bool Input::TriggerKey(BYTE keyNumber){
	if (key_[keyNumber] && !keyPre_[keyNumber]){
		return true;
	}
	return false;
}
// 押し続けを再入力扱いしないため、全キーの今回と前回だけを比較します。
bool Input::TriggerAnyKeyExcept(BYTE excludedKey) const
{
	for (int index = 0; index < 256; ++index) {
		if (index != excludedKey && key_[index] && !keyPre_[index]) {
			return true;
		}
	}
	return false;
}
// 前フレームとの差を調べ、パッドのボタンを一回だけ反応させます。
bool Input::TriggerPadButton(int padIndex, int button) {
	// 指定されたインデックスが範囲外なら拒否
	if (padIndex < 0 || padIndex >= static_cast<int>(padStates_.size()) || button < 0 || button >= 32) return false;

	// 「現在のフレームで押されていて」かつ「前のフレームでは押されていなかった」なら true
	if (padStates_[padIndex].rgbButtons[button] && !padStatesPre_[padIndex].rgbButtons[button]) {
		return true;
	}

	return false;
}

// 今フレームのマウスボタンを読み、ドラッグなど押し続ける操作に使います。
bool Input::IsMouseButtonPressed(int button) {
	if (button < 0 || button >= 4) return false;
	if (mouseState_.rgbButtons[button] & 0x80) {
		return true;
	}
	return false;
}

// 前フレームとの差を調べ、クリックを押した瞬間だけ成立させます。
bool Input::TriggerMouseButton(int button) {
	if (button < 0 || button >= 4) return false;
	return (mouseState_.rgbButtons[button] & 0x80) && !(mouseStatePre_.rgbButtons[button] & 0x80);
}

// 今フレームのパッドボタンを読み、押し続ける操作に使います。
bool Input::IsPadButtonPressed(int padIndex, int button) {
	if (padIndex < 0 || padIndex >= static_cast<int>(padStates_.size()) || button < 0 || button >= 32) return false;

	if (padStates_[padIndex].rgbButtons[button]) {
		return true;
	}

	return false;
}

// 左スティックの横方向を読み、共通の範囲と遊び幅で判定します。
float Input::GetPadLeftAxisX(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates_.size())) {
		return NormalizePadAxis(padStates_[padIndex].lX);
	}
	return 0.0f;
}
// 左スティックの縦方向を読み、横方向と同じ遊び幅で判定します。
float Input::GetPadLeftAxisY(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates_.size())) {
		// 符号はここで反転せず、DirectInputから取得した向きをそのまま返します。
		return NormalizePadAxis(padStates_[padIndex].lY);
	}
	return 0.0f;
}

// 右スティックの横方向を読み、左スティックと同じ基準で判定します。
float Input::GetPadRightAxisX(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates_.size())) {
		return NormalizePadAxis(padStates_[padIndex].lRx);
	}
	return 0.0f;
}

// 右スティックの縦方向を読み、他の軸と同じ基準で判定します。
float Input::GetPadRightAxisY(int padIndex) {
	if (padIndex >= 0 && padIndex < static_cast<int>(padStates_.size())) {
		return NormalizePadAxis(padStates_[padIndex].lRy);
	}
	return 0.0f;
}
} // namespace Engine
