#include "WinApp.h"

#include "ImGuiManager.h"

#pragma comment(lib, "winmm.lib")

#ifdef USE_IMGUI
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

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

void WinApp::Initialize()
{
	isComInitialized_ = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"CG2WindowClass";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);

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
	hwnd = CreateWindow(
		wc.lpszClassName,
		L"CG2",
		WS_OVERLAPPEDWINDOW,
		windowedRect_.left,
		windowedRect_.top,
		windowedRect_.right - windowedRect_.left,
		windowedRect_.bottom - windowedRect_.top,
		nullptr,
		nullptr,
		wc.hInstance,
		this);

	ShowWindow(hwnd, SW_MAXIMIZE);
	timeBeginPeriod(1);
}

void WinApp::Update()
{
}

void WinApp::Finalize()
{
	timeEndPeriod(1);
	CloseWindow(hwnd);
	if (isComInitialized_) {
		CoUninitialize();
		isComInitialized_ = false;
	}
}

bool WinApp::ProcessMessage()
{
	MSG msg{};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT) {
			return true;
		}
	}
	return false;
}

void WinApp::RequestClose()
{
	if (hwnd) {
		PostMessage(hwnd, WM_CLOSE, 0, 0);
	}
}

void WinApp::SetWindowMode(WindowMode mode)
{
	if (!hwnd || mode == GetWindowMode()) {
		return;
	}

	if (GetWindowMode() == WindowMode::Windowed) {
		GetWindowRect(hwnd, &windowedRect_);
	}

	if (mode == WindowMode::BorderlessFullscreen) {
		ShowWindow(hwnd, SW_RESTORE);
		SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP);
		MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
		GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitorInfo);
		SetWindowPos(
			hwnd, HWND_TOP,
			monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
			monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
			monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	} else {
		SetWindowLongPtr(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		SetWindowPos(
			hwnd, nullptr,
			windowedRect_.left, windowedRect_.top,
			windowedRect_.right - windowedRect_.left,
			windowedRect_.bottom - windowedRect_.top,
			SWP_FRAMECHANGED | SWP_NOZORDER | SWP_SHOWWINDOW);
		ShowWindow(hwnd, mode == WindowMode::Maximized ? SW_MAXIMIZE : SW_RESTORE);
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

uint32_t WinApp::GetClientWidth() const
{
	RECT rect{};
	GetClientRect(hwnd, &rect);
	return static_cast<uint32_t>(rect.right - rect.left);
}

uint32_t WinApp::GetClientHeight() const
{
	RECT rect{};
	GetClientRect(hwnd, &rect);
	return static_cast<uint32_t>(rect.bottom - rect.top);
}
