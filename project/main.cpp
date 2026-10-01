#include <Windows.h>
#include "Game.h"
#include <memory>

#pragma comment(lib,"Dbghelp.lib")
#pragma comment(lib,"dxguid.lib")

using namespace Microsoft::WRL;

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	struct D3DResourceLeakChecker
	{
		~D3DResourceLeakChecker(){}
	}leakChecker;
	
	//インスタンス生成
	std::unique_ptr<Framework> game = std::make_unique<Game>();

	//実行
	game->Run();

	return 0;
}
