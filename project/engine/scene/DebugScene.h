#pragma once
#include "Object3d.h"
#include "../debug/GameViewCapture.h"
#include "../debug/DebugParticleEffects.h"
#include "../debug/DebugAnimationPreview.h"
#include "../debug/DebugAssetPreview.h"
#include "../debug/DebugSceneEditor.h"
#include "../debug/DebugSceneContent.h"
#include "../debug/DebugEntityRegistry.h"
#include "../debug/DebugLevelRuntime.h"
#include "../debug/DebugGameViewCameraController.h"
#include "../debug/DebugSceneSelection.h"
#include "../debug/DebugTimePlaybackSmoke.h"
#include "../debug/DebugUiSmoke.h"
#include"BaseScene.h"
#include <string>
#include <vector>
#include <memory>

class Camera;
class ParticleEmitter;
class SkyBox;
class Sprite;

//BaseSceneを継承する(publicをつけることで公認の親子関係)
class DebugScene : public BaseScene
{
public:
	// 前方宣言した所有型を安全に扱うため、実装はDebugScene.cppに置きます。
	DebugScene();
	// unique_ptrが前方宣言したDebug用型を安全に解放できるよう、実装はDebugScene.cppに置きます。
	~DebugScene() override;
	// Debug画面へ入った一度だけ、確認用のCamera・モデル・UIを作成します。
    void Initialize()override;
	// Debug画面を抜ける時に、DebugSceneが所有するデータを解放します。
	void Finalize()override;
	// 毎フレーム、確認用モデル・入力・ImGuiを更新します。
	void Update()override;
	// 毎フレーム、確認用モデル・Sprite・ImGuiを描画します。
	void Draw()override;

private:
	// ---------- DebugScene開始時の準備 ----------

	// PostEffect・Object3d・Sprite・Particleの共通状態を、Debug用の初期値へそろえます。
	void InitializeRenderSystems();
	// MainCameraと上から見る補助Cameraを作り、Scene専用のCameraManagerへ登録します。
	void InitializeCameras();
	// Texture・SkyBox・Audio・Particle素材など、初期Objectより先に必要な素材を読み込みます。
	void InitializeSceneResources();
	// Terrain・Animation確認モデル・scene.json・初期Spriteを、DebugScene所有の一覧へ追加します。
	bool InitializeInitialContent();
	// Debug画面全体で使うLightと、Particle Emitterを初期値で作成します。
	void InitializeLightsAndEmitters();
	// 初期ObjectをECSへ登録し、Shelf・Capture・自動確認を開始します。
	void InitializeDebugTools();
	// Camera・Particle・3D/2Dモデルを、現在のScene設定で更新します。
	void UpdateSceneContent();
	// Preview復帰、Editor、Skeleton表示を含むEdit Viewの操作を更新します。
	void UpdateEditorUi();
	// UI・時間再生の自動確認とParticle Effectを、通常更新の最後に進めます。
	void UpdateAutomation();

	// ---------- ImGuiの確認・編集UI ----------

	// Debug編集UI部品へ、Sceneが所有するデータと操作窓口を渡します。
	DebugSceneEditor::Context MakeEditorContext();

	// ---------- モデル・Textureのプレビュー ----------
	// 指定モデルだけを見るプレビューモードへ入ります。
    bool EnterModelPreview(const std::string& fileName);
	// 指定Textureだけを見るプレビューモードへ入ります。
    bool EnterTexturePreview(const std::string& textureFilePath);
	// モデル・Textureのプレビューモードを終了します。
    void ExitModelPreview();
	// Debug用モデル・Texture生成部品へ、Sceneが所有する一覧・ECS・選択の窓口を渡します。
	DebugSceneContent::Context MakeContentContext();
	// Debug用Level読込部品へ、Sceneが所有するObject・ECS・選択状態の窓口を渡します。
	DebugLevelRuntime::Context MakeLevelRuntimeContext();
	// ---------- Particle確認 ----------

	// Particleを出す基準となる3D座標を返します。
    Vector3 GetParticleEffectPosition() const;

	// ---------- 自動確認用テスト ----------

	// UI操作の自動確認を環境変数から開始します。
    void InitializeUiSmokeFromEnvironment();
	// UI操作の自動確認へ渡すDebugSceneのモデル・プレビュー操作窓口を作ります。
	DebugUiSmoke::Context MakeUiSmokeContext();
	// 時間・Animation・Particleの自動確認を環境変数から開始します。
    void InitializeTimePlaybackSmokeFromEnvironment();
	// 自動確認へ渡すDebugSceneのモデル操作窓口を作ります。
	DebugTimePlaybackSmoke::Context MakeTimePlaybackSmokeContext();

    // ---------- Debug用Camera ----------

    // 上から確認する時に使える補助Cameraです。
    std::unique_ptr<Camera> upCamera_;

    // ---------- Scene内の3Dモデル・Sprite・ECS ----------

    // Animationを使わない3Dモデルです。
    std::vector<std::unique_ptr<Object3d>> normalObjects_;
    // Animationを再生する3Dモデルです。
    std::vector<std::unique_ptr<Object3d>> animationObjects_;
	// 歩行Animation・手持ちWeapon・足跡Particleをまとめて確認するDebug部品です。
	DebugAnimationPreview debugAnimationPreview_{};

	// Debug画面へ追加した2D画像です。
    std::vector<std::unique_ptr<Sprite>> sprites_;
	// モデル・SpriteのEntity登録とECS Inspectorの選択状態を担当します。
	DebugEntityRegistry entityRegistry_{};
	// 背景として描画するSkyBoxです。
    std::unique_ptr<SkyBox> skyBox_;

	// ---------- モデル棚とプレビュー ----------

	// Inspector・Model Shelf・Edit Viewをまとめて表示するDebug用編集部品です。
	DebugSceneEditor debugSceneEditor_{};
	// モデル・Textureプレビューと専用Camera操作を担当するDebug用部品です。
	DebugAssetPreview assetPreview_{};

    // ---------- scene.jsonのHot Reload ----------

	// scene.jsonのJSON由来モデル範囲とHot Reloadを担当するDebug用部品です。
	DebugLevelRuntime levelRuntime_{};
	// 初期配置のAnimationモデル数です。
    size_t baseAnimationObjectCount_ = 0;
	// 初期配置のSprite数です。
    size_t baseSpriteCount_ = 0;

	// Game Viewの画像保存・動画記録・リプレイ保存を担当します。
	GameViewCapture gameViewCapture_{};

	// ---------- 3D・2D選択状態 ----------

	// Inspector・Edit Viewで選んだ3DモデルとSpriteの番号を管理します。
	DebugSceneSelection selection_{};

    // ---------- ParticleのEmitter ----------

	// 円形Particleを発生させるEmitterです。
    std::unique_ptr<ParticleEmitter> emitterCircle_;
	// 平面Particleを発生させるEmitterです。
    std::unique_ptr<ParticleEmitter> emitterPlane_;

	// Emitter位置の基準にする平面モデルへの非所有ポインタです。
    Object3d* objectPlane_ = nullptr;
	// Inspectorで現在選択しているEmitterへの非所有ポインタです。
    ParticleEmitter* activeEmitter_ = nullptr;

    // ---------- Debug画面全体のライト ----------

	// 太陽光のように、全モデルへ同じ方向から当てる光です。
    Object3d::DirectionalLight directionalLight_;
	// 指定した一点から周囲へ広がる光です。
    Object3d::PointLight pointLight_;
	// 円すい状の範囲だけを照らす光です。
    Object3d::SpotLight spotLight_;

    // ---------- Particleの見た目設定 ----------

	// Emitterを置く位置・回転・大きさです。
    Transform emitterTransform_{};

	// Debug用Particleの設定・発生状態です。
	// DebugSceneは発生位置を渡すだけで、Particle種類ごとの設定を持ちません。
	DebugParticleEffects debugParticleEffects_{};
	// Debug Game ViewのMouse Camera操作状態を担当する部品です。
	DebugGameViewCameraController gameViewCameraController_{};

    // ---------- UI操作の自動確認 ----------

	// テスト段階・ログ・保存待ち状態は、Debug専用クラスが所有します。
	DebugUiSmoke::State uiSmoke_{};

    // ---------- 時間・Animation・Particleの自動確認 ----------

	// テスト段階・ログ・一時的なTransformは、Debug専用クラスが所有します。
	DebugTimePlaybackSmoke::State timePlaybackSmoke_{};
};

