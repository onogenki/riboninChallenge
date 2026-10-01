#pragma once
#include "BaseScene.h"
#include "AbstractSceneFactory.h"
#include<string>

class SceneManager
{
public:

	// インスタンス取得関数
	static SceneManager* GetInstance();

	void Update();
	void Draw();

	//シーンファクトリーのsetter
	void SetSceneFactory(AbstractSceneFactory* sceneFactory) { sceneFactory_ = sceneFactory; }

	//次シーン予約
	bool ChangeScene(const std::string& sceneName);
	// 白いLoadingSceneを一度表示してから、指定したSceneへ切り替えます。
	bool ChangeSceneWithLoading(const std::string& sceneName);
	// LoadingSceneだけが一度取得する、Loading完了後の切替先です。
	std::string TakeLoadingDestinationSceneName();
	bool RestartCurrentScene();
	void FinalizeCurrentScene();

	const std::string& GetCurrentSceneName() const { return currentSceneName_; }
	const std::string& GetPendingSceneName() const { return pendingSceneName_; }
	const std::vector<std::string>& GetAvailableSceneNames() const;
	bool HasPendingScene() const { return nextScene_ != nullptr || isChangingScene_ || sceneChangeCooldownFrames_ > 0; }

private:
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

