#pragma once

#include "Object3d.h"
#include <memory>
#include <vector>

class Object3dRenderContext;
class Object3dCommon;
class Camera;

// Debug画面で歩行Animation、手持ちモデル、足跡Particleを確認する部品です。
// 人型・歩行モデルの寿命はDebugSceneのAnimation一覧が持ち、Weaponだけはこの部品が所有します。
class DebugAnimationPreview
{
public:
	struct Context
	{
		// Animationを更新するモデル一覧です。所有しません。
		std::vector<std::unique_ptr<Object3d>>* animationObjects = nullptr;
		// Animation後の行列・Camera・Lightをモデルへ設定する共通部品です。所有しません。
		Object3dRenderContext* renderContext = nullptr;
		// 足跡Particleへ渡す共通の平行光です。所有しません。
		const Object3d::DirectionalLight* directionalLight = nullptr;
		// このフレームの経過時間です。
		float deltaTime = 0.0f;
		// trueならWASDで歩行モデルを操作できます。
		bool acceptsGameInput = false;
	};

	// 人型・歩行モデル・手持ちWeaponを作り、Animation確認を始める準備をします。
	bool Initialize(
		Object3dCommon* object3dCommon,
		std::vector<std::unique_ptr<Object3d>>& animationObjects);
	// 歩行入力、Animation行列、手持ちモデル、足跡Particleを順番に更新します。
	void Update(const Context& context);
	// Particleや自動確認が参照する、人型Animationモデルを返します。所有権は渡しません。
	Object3d* GetAttachmentSource() const { return attachmentSource_; }
	// 描画用に、この部品が所有する手持ちWeaponを返します。所有権は渡しません。
	Object3d* GetHandWeapon() const { return handWeapon_.get(); }
	// 人型モデル付近へParticleを出すための基準座標を返します。
	Vector3 GetParticleEffectPosition() const;
	// Skeleton表示が有効な時だけ、人型Animationの骨をGame Viewへ重ねて描画します。
	void DrawSkeletonDebug(const Camera* camera, bool isEnabled) const;
	// Scene終了時に、非所有ポインタとWeaponをまとめて解放します。
	void Finalize();

private:
	// Weaponを手ボーンへ付ける、人型Animationモデルへの非所有ポインタです。
	Object3d* attachmentSource_ = nullptr;
	// WASDで歩かせる、足跡確認用モデルへの非所有ポインタです。
	Object3d* walkObject_ = nullptr;
	// 人型の手ボーンへ付ける確認用モデルです。
	std::unique_ptr<Object3d> handWeapon_;
	// 歩行用モデルへ再生させるAnimationデータです。
	Model::Animation walkAnimation_{};
	// 人型モデルへ再生させるAnimationデータです。
	Model::Animation humanAnimation_{};
};
