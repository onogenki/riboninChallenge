#pragma once

#include "LaserRenderer.h"
#include "Object3d.h"
#include <memory>
#include <vector>

class Player;
namespace Engine { class Camera; }
namespace Engine { class DirectXCommon; }
namespace Engine { class Sprite; }
namespace Engine { class SpriteCommon; }

// Goalのくぼみから出る光、歩ける台座、クリア時の上昇と白い画面を所有します。
class StageGoal
{
public:
	StageGoal();
	~StageGoal();
	// @brief 描画資源を一度だけ準備し、Stageの初期化結果へ反映します。
	// @param directXCommon Laser描画に使うDirectXの共通資源です。nullptrは受け付けません。
	// @param spriteCommon 白画面に使うSpriteの共通資源です。nullptrは受け付けません。
	// @return 光と白画面の準備がすべて成功した時だけtrueです。
	bool Initialize(Engine::DirectXCommon* directXCommon, Engine::SpriteCommon* spriteCommon);
	// @brief Goalモデルの現在位置と倍率に、光と台座の基準を合わせます。
	// @param transform JSONやEditorで更新されたGoalモデルのTransformです。
	void SetGoalTransform(const Transform& transform);
	// Goalを無効にし、演出中の状態と光の線分も消します。
	void ClearGoal();
	// 台座の断面に沿ってPlayerの接地位置を補正します。
	void ApplyWalkableSurface(Player& player) const;
	// Playerの中心がくぼみから立つ白い光へ入った時だけtrueです。
	bool IsPlayerInLight(const Player& player) const;
	// 入光時のPlayerとCameraの位置を記録し、通常操作から演出へ切り替えます。
	void Begin(const Player& player, const Engine::Camera& camera);
	// @brief Player、Camera、白画面の順にクリア演出を進めます。
	// @param player 上昇位置と見た目を更新するPlayerです。
	// @param camera Playerを追う位置と向きを更新するCameraです。
	// @param deltaTime 今回進める時間を秒で渡します。
	// @return 演出中に切替時刻へ到達した時だけtrueです。演出していない時はfalseです。
	bool Update(Player& player, Engine::Camera& camera, float deltaTime);
	bool IsPlaying() const { return isPlaying_; }
	// Playerの影に掛ける表示量を返し、上昇中に徐々に小さくします。
	float GetShadowVisibility() const;
	// StageLightingへ渡す、くぼみから真上へ向かう最優先の白いSpotLightです。
	Engine::Object3d::SpotLight GetSpotLight() const;
	// StageSceneRendererが通常3Dの後に加算光を描きます。
	void DrawLight(const Engine::Camera& camera);
	// StageのSprite描画経路へ、一枚の白い画面を貸します。
	const std::vector<std::unique_ptr<Engine::Sprite>>& GetOverlaySprites() const { return overlaySprites_; }
	bool IsOverlayVisible() const { return isPlaying_ && overlayAlpha_ > 0.0f; }

private:
	// クリア演出の経過時間に合わせて、Playerの位置と見た目を更新します。
	void UpdatePlayerPresentation(Player& player);
	// 更新済みPlayerを見ながらCameraを光へ近づけます。
	void UpdateCameraPresentation(const Player& player, Engine::Camera& camera);
	// 最後に白画面を更新し、Camera移動の後にフェードを進めます。
	void UpdateOverlayPresentation();

	Vector3 goalCenter_{};
	Vector3 goalScale_{ 1.0f, 1.0f, 1.0f };
	Vector3 playerStart_{};
	Vector3 cameraStart_{};
	Vector3 cameraTarget_{};
	float elapsedTime_ = 0.0f;
	float overlayAlpha_ = 0.0f;
	bool hasGoal_ = false;
	bool isPlaying_ = false;
	LaserRenderer outerLight_{};
	LaserRenderer innerLight_{};
	std::vector<LaserSegment> lightSegments_;
	std::vector<std::unique_ptr<Engine::Sprite>> overlaySprites_;
};
