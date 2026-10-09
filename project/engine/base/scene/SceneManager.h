#pragma once
#include "BaseScene.h"
#include "AbstractSceneFactory.h"
#include<string>

// 現在Sceneを状態として所有し、Factoryが作った次Sceneへ安全に切り替えるState Patternの本体です。
class SceneManager
{
public:

	// アプリ全体で共有するSceneManagerを返します。本体の解放は呼び出し元へ任せません。
	static SceneManager* GetInstance();

	// 予約済みSceneを確定してから現在Sceneを更新し、切替待機を1フレーム進めます。
	void Update();
	// 現在Sceneがあれば描画し、描画後のCaptureも更新します。
	void Draw();

	// Sceneを生成するFactoryを借ります。呼び出し元がFactoryの寿命を管理します。
	void SetSceneFactory(AbstractSceneFactory* sceneFactory) { sceneFactory_ = sceneFactory; }

	// sceneNameのSceneを次フレームへ予約し、予約できた時だけtrueを返します。
	bool ChangeScene(const std::string& sceneName);
	// Loading自身以外の登録済みsceneNameだけを予約し、失敗時は白画面へ切り替えません。
	bool ChangeSceneWithLoading(const std::string& sceneName);
	// Loadingの切替先を一度だけ取り出し、Manager側の登録を空にして返します。
	std::string TakeLoadingDestinationSceneName();
	// 現在のScene名で作り直しを予約し、予約できた時だけtrueを返します。
	bool RestartCurrentScene();
	// 終了時にGPUの使用完了を待ち、現在と予約中のSceneを解放します。
	void FinalizeCurrentScene();

	// 現在または予約中のScene名を、Managerが保持している間だけ参照で返します。
	const std::string& GetCurrentSceneName() const { return currentSceneName_; }
	const std::string& GetPendingSceneName() const { return pendingSceneName_; }
	// Factoryが登録したScene名一覧を借りて返し、Factoryがなければ空一覧を返します。
	const std::vector<std::string>& GetAvailableSceneNames() const;
	// 次Sceneの予約・切替処理・待機期間のいずれかが残っている時にtrueを返します。
	bool HasPendingScene() const { return nextScene_ != nullptr || isChangingScene_ || sceneChangeCooldownFrames_ > 0; }

private:
	// 連続したScene切替を避け、Loadingを描画できる期間を確保する待機フレーム数です。
	static constexpr int kSceneChangeCooldownFrames = 6;

	// 予約済みSceneを安全に終了・初期化して、現在Sceneとして確定します。
	void ApplyPendingSceneChange();

	// シングルトン化：コンストラクタとデストラクタをprivateにする
	SceneManager() = default;
	~SceneManager();

	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	//今のシーン
	std::unique_ptr<BaseScene> scene_ = nullptr;
	//次のシーン
	std::unique_ptr<BaseScene> nextScene_ = nullptr;
	std::string currentSceneName_;
	std::string pendingSceneName_;
	// LoadingSceneが表示を終えた後に開く、次のScene名を一時的に保持します。
	std::string loadingDestinationSceneName_;
	bool isChangingScene_ = false;
	int sceneChangeCooldownFrames_ = 0;

	//シーンファクトリー(借りてくる)
	AbstractSceneFactory* sceneFactory_ = nullptr;
};

