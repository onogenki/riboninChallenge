#pragma once

#include "Collider.h"
#include "LevelLoader.h"
#include "Object3d.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Object3dRenderContext;
class Camera;

// LevelLoaderが読んだ通常3Dモデルを、ゲーム中のObject3dとして管理するクラスです。
// Player、Camera、鏡などのゲーム固有処理は持たず、Stage1やStage2から共通利用できます。
class StageMapRuntime
{
public:
	// JSONまたはCSVの一件と、実行中のモデル・当たり判定を結び付けます。
	struct RuntimeObject
	{
		// JSONまたはCSV上のnameです。Editorで対象を見分けるために使います。
		std::string sourceName;
		// JSON上のtagです。SceneがCameraBoundaryなどの役割で振り分けるために使います。
		std::string tag;
		// 画面に描画する3Dモデルです。
		std::unique_ptr<Object3d> visual;
		// モデルのTransformを反映して、実際に衝突判定へ使うOBB Colliderです。
		ObbCollider collider{};
		// trueならBOX Colliderが設定されています。
		bool hasBoxCollider = false;
		// 制御点移動を始める前のモデル位置です。
		Vector3 pathBasePosition{};
		// JSONに書いた移動経路の点です。
		std::vector<Vector3> controlPoints;
		// 一秒あたりに移動経路を進む速さです。
		float pathSpeed = 1.0f;
		// 現在、移動経路のどこまで進んだかを表します。
		float pathProgress = 0.0f;
		// trueなら終点に着いた後、始点へ戻って繰り返します。
		bool pathLoop = true;

		// Playerなどが配置物のColliderへ判定を依頼する時に使います。
		const ObbCollider& GetCollider() const { return collider; }
		// Sceneの床・壁一覧やLaser遮蔽へ渡す、同期済みのOBB形状データを返します。
		const MyMath::OBB& GetObb() const { return collider.GetShape(); }
	};

	// Sceneが持つ照明設定を保ったObject3dを作る関数です。
	using CreateObjectFunction = std::function<std::unique_ptr<Object3d>(const std::string& modelName)>;

	// 読み込んだマップデータから、通常3Dモデルをすべて作り直します。
	bool Rebuild(
		const std::vector<const LevelLoader::ObjectData*>& objectDataList,
		const CreateObjectFunction& createObject);
	// 既存モデルを作り直さず、Editorで変更したTransformなどだけを反映します。
	void ApplyEdits(const std::vector<const LevelLoader::ObjectData*>& objectDataList);
	// control_pointsを持つモデルを、経過時間に合わせて移動します。
	void UpdatePaths(float deltaTime);
	// Sceneが一度作ったCamera・Light設定を、Runtimeが所有する全モデルへ反映します。
	void UpdateRenderObjects(const Object3dRenderContext& renderContext);
	// Mirror反射Cameraまたは通常Cameraの行列を、Runtimeが所有する全モデルへ反映します。
	void UpdateCameraForDraw(Camera* camera);
	// Object3d用Pipeline設定後に、Runtimeが所有する全モデルを描画します。
	void Draw() const;
	// Editor変更・移動経路で変わったモデルTransformをColliderへ反映します。
	void SyncColliders();
	// Stage終了時に、実行中のモデルをすべて解放します。
	void Clear();

	// Sceneが描画・当たり判定・Editor表示に使う実行中オブジェクト一覧です。
	std::vector<RuntimeObject>& GetObjects() { return objects_; }
	const std::vector<RuntimeObject>& GetObjects() const { return objects_; }

private:
	// JSON/CSVから生成した通常3Dモデルの一覧です。
	std::vector<RuntimeObject> objects_;
};
