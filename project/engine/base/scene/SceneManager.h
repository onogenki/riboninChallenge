#pragma once
#include "BaseScene.h"
#include "AbstractSceneFactory.h"
#include <string>

namespace Engine
{

class Sprite;

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

	// @brief Sceneを生成するFactoryを借ります。Managerは所有しません。
	// @param sceneFactory Scene切替とManagerの利用中に生存するFactoryです。
	void SetSceneFactory(AbstractSceneFactory* sceneFactory) { sceneFactory_ = sceneFactory; }

	// @brief 指定名のSceneをFactoryで生成し、次のUpdateで切り替える予約をします。
	// @param sceneName Factoryに登録済みのScene名です。
	// @return 予約できた時だけtrueです。Sceneの初期化完了や描画成功は示しません。
	bool ChangeScene(const std::string& sceneName);
	// @brief Loading自身以外の登録済みSceneだけを、白いLoading経由で予約します。
	// @param sceneName Loading後に開く、Factory登録済みのScene名です。
	// @return Loadingの予約に成功した時だけtrueです。行き先の初期化完了は示しません。
	bool ChangeSceneWithLoading(const std::string& sceneName);
	// @brief 現在画面を白く覆い、Loadingと準備待ちを経て行き先を徐々に表示します。
	// @param sceneName Loading以外のFactory登録済みのScene名です。
	// @return フェード開始を受け付けた時だけtrueです。遷移完了は示しません。
	bool ChangeSceneWithFade(const std::string& sceneName);
	// SceneRenderPipelineが最後に白Spriteを重ねるための描画口です。
	void DrawFadeOverlay();
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

	// 予約済みSceneを初期化し、失敗時は未完成Sceneを更新・描画へ渡さずfalseを返します。
	bool ApplyPendingSceneChange();
	// Scene切替の状態と白Spriteの透明度を、更新順に沿って進めます。
	void UpdateFade();
	// 画面全体を覆うSpriteを一度だけ生成します。
	bool PrepareFadeSprite();

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
	enum class FadePhase { kNone, kFadeIn, kWaitingForDestination, kFadeOut };
	FadePhase fadePhase_ = FadePhase::kNone;
	std::string fadeDestination_;
	std::unique_ptr<Engine::Sprite> fadeSprite_;
	float fadeAlpha_ = 0.0f;
	bool destinationDrawn_ = false;
};
}

