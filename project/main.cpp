#include <Windows.h>
#include "Game.h"
#include <memory>

#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"dxguid.lib")

// Gameを唯一の実行主体として所有し、終了時に確実に破棄するWindowsの入口です。
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	//インスタンス生成
	std::unique_ptr<Framework> game = std::make_unique<Game>();

	//実行
	game->Run();

	return 0;
}
