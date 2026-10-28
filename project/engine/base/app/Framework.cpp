#include "Framework.h"
#include "Logger.h"

#include <filesystem>
#include <chrono>
#include <exception>
#include <fstream>
#include <format>
#include <mfapi.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>
#include <system_error>

#include "TextureManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "GPUParticle.h"
#include "PostEffect.h"
#include "Audio.h"
#include "AbstractSceneFactory.h"
#include "CaptureManager.h"

#pragma comment(lib, "Dbghelp.lib")

namespace {
void AlignWorkingDirectoryToResources()
{
	std::error_code errorCode;
	std::filesystem::path directory = std::filesystem::current_path(errorCode);
	if (errorCode) {
		return;
	}

	while (!directory.empty()) {
		const std::filesystem::path directResources = directory / "resources" / "shaders";
		if (std::filesystem::exists(directResources, errorCode) && !errorCode) {
			std::filesystem::current_path(directory, errorCode);
			return;
		}
		errorCode.clear();

		const std::filesystem::path projectDirectory = directory / "project";
		const std::filesystem::path projectResources = projectDirectory / "resources" / "shaders";
		if (std::filesystem::exists(projectResources, errorCode) && !errorCode) {
			std::filesystem::current_path(projectDirectory, errorCode);
			return;
		}
		errorCode.clear();

		if (directory == directory.root_path()) {
			break;
		}
		directory = directory.parent_path();
	}
}
}

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception)
{
	//時刻を取得して、時刻を名前に入れたファイルを作成。
	// Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	//processId(このexeのID)とクラッシュ(例外)の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	//設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	//Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
	//他に関連づけられていSEH例外ハンドラあれば実行。通常はプロセスを終了する
	return EXCEPTION_EXECUTE_HANDLER;
}

void Framework::Initialize()
{
	AlignWorkingDirectoryToResources();

	//ログのディレクトリを用意
	std::filesystem::create_directory("logs");

	//ここからファイルを作成し
	//現在時刻を取得(UTC時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	//ログファイルの名前にコンマ何秒はいらないので削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	//日本時間(pcの設定時間)に変換
	std::chrono::zoned_time localTime{ std::chrono::current_zone(),nowSeconds };
	//formatを使って年月日_時分秒の文字列に変換
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	//時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + dateString + ".log";
	//ファイルを作って書き込み準備
	Logger::SetLogPath(logFilePath);

	Logger::Log("\nHello DirectX!\n");

	winApp_ = std::make_unique<WinApp>();
	winApp_->Initialize();

	// GPU資源の生成失敗も後段と同じ終了経路へ渡し、未完成のHeapやRTVを使いません。
	try {
		//DirectXの初期化
		dxCommon_ = DirectXCommon::GetInstance();
		dxCommon_->Initialize(winApp_.get());

		//SRVマネージャの初期化
		srvManager_ = SrvManager::GetInstance();
		srvManager_->Initialize(dxCommon_);
		// RenderTextureをShaderから読めるよう、SrvManager初期化後にSRVを生成する
		dxCommon_->CreateRenderTextureSRV(srvManager_);
		TextureManager::GetInstance()->Initialize(dxCommon_, srvManager_);
		// 失敗した資源の後続を初期化しないため、Sceneが必要とする共通部品を順に準備します。
		// RenderTextureをSwapChainへコピーするPostEffectを初期化する
		PostEffect::GetInstance()->Initialize(dxCommon_, srvManager_);
		Engine::ModelManager::GetInstance()->Initialize(dxCommon_);
		Engine::ParticleManager::GetInstance()->Initialize(dxCommon_, srvManager_);
		Engine::GPUParticle::GetInstance()->Initialize(dxCommon_, srvManager_);
		Audio::GetInstance()->Initialize();
		isAudioInitialized_ = true;

		// 入力とImGuiは描画資源の準備後に従来どおり初期化します。
		input_ = Input::GetInstance();
		input_->Initialize(winApp_.get());
		imGuiManager_ = ImGuiManager::GetInstance();
		imGuiManager_->Initialize(winApp_.get(), dxCommon_, srvManager_);
		isImGuiInitialized_ = true;
		CaptureManager::GetInstance()->Initialize();
	} catch (const std::exception& error) {
		// GPU資源・必須画像・Shaderが欠けた時は、次の初期化とScene開始を止めます。
		Logger::Log(std::string("Framework initialization failed: ") + error.what());
		RequestEnd(1);
	}

	// クラッシュ時にExportDumpを呼ぶように登録
	SetUnhandledExceptionFilter(ExportDump);
}

// SceneFactoryを先に所有し、SceneManagerが参照する間は破棄されないようにします。
void Framework::SetSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory)
{
	sceneFactory_ = std::move(sceneFactory);
	Engine::SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
}

// 終了判定と終了コードを一緒に更新し、派生先が内部状態を書き換えないようにします。
void Framework::RequestEnd(int exitCode)
{
	exitCode_ = exitCode;
	endRequest_ = true;
}

// 終了メッセージを受けたフレームでは、破棄済みWindowを使う更新へ進みません。
void Framework::Update()
{
	// Windowを閉じた後のResize・Input・Scene更新を避けるため、ここで戻ります。
	if (winApp_->ProcessMessage()) {
		RequestEnd();
		return;
	}
	// Resize失敗時は古い描画先へ描画せず、後続のInput・Scene更新も止めます。
	try {
		dxCommon_->ResizeIfNeeded();
	} catch (const std::exception& error) {
		Logger::Log(std::string("Framework resize failed: ") + error.what());
		RequestEnd(1);
		return;
	}
	input_->Update();
	Audio::GetInstance()->Update();
	PostEffect::GetInstance()->Update(dxCommon_->GetDeltaTime());
	// モデル・Textureを保存したフレームだけ再読込するため、すべてのSceneで同じ確認を行います。
	TextureManager::GetInstance()->UpdateHotReload();
	Engine::ModelManager::GetInstance()->UpdateHotReload();

	//マネージャーにシーンの更新(と切り替え)を任せる
	Engine::SceneManager::GetInstance()->Update();
}

// Sceneを先に終了し、共有資源は使う側から作成時と逆の順で解放します。
void Framework::Finalize()
{

	Engine::SceneManager::GetInstance()->FinalizeCurrentScene();
	Engine::SceneManager::GetInstance()->SetSceneFactory(nullptr);

	CaptureManager::GetInstance()->Finalize();
	if (isImGuiInitialized_) {
		imGuiManager_->Finalize();
		isImGuiInitialized_ = false;
	}
	Engine::GPUParticle::GetInstance()->Finalize();
	Engine::ParticleManager::GetInstance()->Finalize();
	Engine::ModelManager::GetInstance()->Finalize();
	PostEffect::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	if (isAudioInitialized_) {
		Audio::GetInstance()->Unload();
		// Audio::Initializeで開始したMedia Foundationは、音源の解放後に終了します。
		MFShutdown();
		isAudioInitialized_ = false;
	}

	if (winApp_) {
		winApp_->Finalize();
	}

	//シーンファクトリー解放
	sceneFactory_.reset();
	winApp_.reset();
}

// 終了理由を資源解放前に取り出し、WinMainへ伝えます。
int Framework::Run()
{
	//ゲームの初期化
	Initialize();

	// 初期化失敗を受け取った時は、未完成のSceneを更新・描画せず解放へ進みます。
	while (!IsEndRequest()) //ゲームループ
	{
		//毎フレーム更新
		Update();
		//終了リクエストがきたら抜ける
		if (IsEndRequest())
		{
			break;
		}
		//描画
		Draw();
	}
	// WinAppはFinalizeで解放されるため、先に終了コードを保存します。
	const int result = exitCode_ != 0 ? exitCode_ : (winApp_ ? winApp_->GetQuitCode() : 0);
	//ゲームの終了
	Finalize();
	return result;
}
