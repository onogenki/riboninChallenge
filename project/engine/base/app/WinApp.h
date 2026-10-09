#pragma once

#include <Windows.h>
#include <cstdint>

// ゲーム用Windowsウィンドウの生成、メッセージ処理、表示モードを管理します。
class WinApp
{
public:
	// ゲームウィンドウの表示方法
	enum class WindowMode
	{
		Windowed,
		Maximized,
		BorderlessFullscreen,
	};

	// 通常ウィンドウへ戻すときのクライアント領域
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	void Initialize();
	void Update();
	void Finalize();
	bool ProcessMessage();
	void RequestClose();

	void SetWindowMode(WindowMode mode);

	HINSTANCE GetHInstance() const { return wc.hInstance; }
	HWND GetHwnd() const { return hwnd; }
	WindowMode GetWindowMode() const { return windowMode_; }
	// WM_QUITに指定された終了コードを、ウィンドウ解放前にFrameworkへ返します。
	int GetQuitCode() const { return quitCode_; }
	uint32_t GetClientWidth() const;
	uint32_t GetClientHeight() const;

private:
	HWND hwnd = nullptr;
	WNDCLASS wc{};
	WindowMode windowMode_ = WindowMode::Maximized;
	RECT windowedRect_{};
	bool isComInitialized_ = false;
	int quitCode_ = 0;

	void ToggleFullscreen();
};
