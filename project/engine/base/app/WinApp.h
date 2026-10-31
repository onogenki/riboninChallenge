#pragma once

#include <Windows.h>
#include <cstdint>

namespace Engine {

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

	// Windowsから届くメッセージを処理し、未処理分は既定処理の結果を返します。
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	// FrameworkがDirectXとInputを準備する前にWindowを作り、使えるHandleができた時だけtrueを返します。
	bool Initialize();
	// 既存の公開APIを保つための空Updateです。メッセージ処理はProcessMessageで行います。
	void Update();
	// Sceneと描画資源の終了後にWindow側の資源を解放します。
	void Finalize();
	// 待機中のメッセージを処理し、WM_QUITを受けた時だけtrueを返します。
	bool ProcessMessage();
	// WM_CLOSEを予約し、次のProcessMessageで通常の終了経路に入ります。
	void RequestClose();

	// 指定した表示モードに切り替え、通常ウィンドウの位置とサイズを保持します。
	void SetWindowMode(WindowMode mode);

	// 登録済みWindow ClassのInstanceを返し、InputのDirectInput生成に使います。
	HINSTANCE GetHInstance() const { return wc_.hInstance; }
	// Windowの所有権を渡さずHandleだけを返し、入力と描画先の初期化に使います。
	HWND GetHwnd() const { return hwnd_; }
	// 現在の表示モードを返します。Alt+EnterとImGuiの表示切替に使います。
	WindowMode GetWindowMode() const { return windowMode_; }
	// WM_QUITに指定された終了コードを、ウィンドウ解放前にFrameworkへ返します。
	int GetQuitCode() const { return quitCode_; }
	// 現在のクライアント領域の幅を返し、描画先の生成・Resizeに使います。
	uint32_t GetClientWidth() const;
	// 現在のクライアント領域の高さを返し、描画先の生成・Resizeに使います。
	uint32_t GetClientHeight() const;

private:
	HWND hwnd_ = nullptr;
	WNDCLASS wc_{};
	WindowMode windowMode_ = WindowMode::Maximized;
	RECT windowedRect_{};
	bool isComInitialized_ = false;
	bool isTimerPeriodStarted_ = false;
	int quitCode_ = 0;

	void ToggleFullscreen();
};

} // namespace Engine
