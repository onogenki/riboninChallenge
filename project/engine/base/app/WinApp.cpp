#include "WinApp.h"

#include "ImGuiManager.h"
#include "Logger.h"

#pragma comment(lib, "winmm.lib")

#ifdef USE_IMGUI
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace Engine {

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	WinApp* app = reinterpret_cast<WinApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (msg == WM_NCCREATE) {
		const CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lparam);
		app = static_cast<WinApp*>(createStruct->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
	}
	if (msg == WM_SIZE && app && app->windowMode_ != WindowMode::BorderlessFullscreen) {
		app->windowMode_ = (wparam == SIZE_MAXIMIZED) ? WindowMode::Maximized : WindowMode::Windowed;
	}
	if (msg == WM_SYSKEYDOWN && app && wparam == VK_RETURN &&
		(lparam & (1LL << 29)) != 0 && (lparam & (1LL << 30)) == 0) {
		app->ToggleFullscreen();
		return 0;
	}

#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif

	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
}

// COM・Window Class・Handleを順に確認し、描画先に使えないWindowをFrameworkへ渡しません。
bool WinApp::Initialize()
{
	isComInitialized_ = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	if (!isComInitialized_) {
		Logger::Log("WinApp initialization failed: COM could not be initialized.");
		return false;
	}

	wc_.lpfnWndProc = WindowProc;
	wc_.lpszClassName = L"riboninChallengeWindowClass";
	wc_.hInstance = GetModuleHandle(nullptr);
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	if (!RegisterClass(&wc_)) {
		Logger::Log("WinApp initialization failed: Window Class could not be registered.");
		return false;
	}

	RECT windowRect{ 0, 0, kClientWidth, kClientHeight };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);
	MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
	GetMonitorInfo(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &monitorInfo);
	const int windowWidth = windowRect.right - windowRect.left;
	const int windowHeight = windowRect.bottom - windowRect.top;
	windowedRect_.left = monitorInfo.rcWork.left + (monitorInfo.rcWork.right - monitorInfo.rcWork.left - windowWidth) / 2;
	windowedRect_.top = monitorInfo.rcWork.top + (monitorInfo.rcWork.bottom - monitorInfo.rcWork.top - windowHeight) / 2;
	windowedRect_.right = windowedRect_.left + windowWidth;
	windowedRect_.bottom = windowedRect_.top + windowHeight;

	// 起動時は閉じるボタンを使える最大化ウィンドウにする。
	hwnd_ = CreateWindow(
		wc_.lpszClassName,
		L"riboninChallenge",
		WS_OVERLAPPEDWINDOW,
		windowedRect_.left,
		windowedRect_.top,
		windowedRect_.right - windowedRect_.left,
		windowedRect_.bottom - windowedRect_.top,
		nullptr,
		nullptr,
		wc_.hInstance,
		this);

	if (!hwnd_) {
		Logger::Log("WinApp initialization failed: Window could not be created.");
		UnregisterClass(wc_.lpszClassName, wc_.hInstance);
		return false;
	}
	ShowWindow(hwnd_, SW_MAXIMIZE);
	isTimerPeriodStarted_ = timeBeginPeriod(1) == TIMERR_NOERROR;
	return true;
}

void WinApp::Update()
{
}

// 初期化が途中で止まっても、開始済みのWindow・タイマー精度・COMだけを解放します。
void WinApp::Finalize()
{
	if (isTimerPeriodStarted_) {
		timeEndPeriod(1);
		isTimerPeriodStarted_ = false;
	}
	if (hwnd_) {
		CloseWindow(hwnd_);
	}
	if (isComInitialized_) {
		CoUninitialize();
		isComInitialized_ = false;
	}
}

// WM_QUITを通常のWindowProcへ渡さず、終了コードだけをFrameworkへ渡します。
bool WinApp::ProcessMessage()
{
	MSG msg{};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			quitCode_ = static_cast<int>(msg.wParam);
			return true;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return false;
}

// 終了要求をWindowへ送り、通常のMessage経路でゲームループを止めます。
void WinApp::RequestClose()
{
	if (hwnd_) {
		PostMessage(hwnd_, WM_CLOSE, 0, 0);
	}
}

// 現在のWindow Handleを使って表示モードを切り替え、直前の通常サイズを保ちます。
void WinApp::SetWindowMode(WindowMode mode)
{
	if (!hwnd_ || mode == GetWindowMode()) {
		return;
	}

	if (GetWindowMode() == WindowMode::Windowed) {
		GetWindowRect(hwnd_, &windowedRect_);
	}

	if (mode == WindowMode::BorderlessFullscreen) {
		ShowWindow(hwnd_, SW_RESTORE);
		SetWindowLongPtr(hwnd_, GWL_STYLE, WS_POPUP);
		MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
		GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo);
		SetWindowPos(
			hwnd_, HWND_TOP,
			monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	} else {
		SetWindowLongPtr(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowPos(
			hwnd_, nullptr,
			windowedRect_.left, windowedRect_.top,
			windowedRect_.right - windowedRect_.left,
			windowedRect_.bottom - windowedRect_.top,
			SWP_FRAMECHANGED | SWP_NOZORDER | SWP_SHOWWINDOW);
		ShowWindow(hwnd_, mode == WindowMode::Maximized ? SW_MAXIMIZE : SW_RESTORE);
	}

	windowMode_ = mode;
}

void WinApp::ToggleFullscreen()
{
	SetWindowMode(
		GetWindowMode() == WindowMode::BorderlessFullscreen
		? WindowMode::Windowed
		: WindowMode::BorderlessFullscreen);
}

// 現在のクライアント領域を読み、描画解像度をWindowの実寸に合わせます。
uint32_t WinApp::GetClientWidth() const
{
	RECT rect{};
	GetClientRect(hwnd_, &rect);
	return static_cast<uint32_t>(rect.right - rect.left);
}

// 現在のクライアント領域を読み、縦方向もWindowの実寸に合わせます。
uint32_t WinApp::GetClientHeight() const
{
	RECT rect{};
	GetClientRect(hwnd_, &rect);
	return static_cast<uint32_t>(rect.bottom - rect.top);
}

} // namespace Engine
