#pragma once

#include "Collider.h"
#include "LevelLoader.h"
#include "Object3d.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class Object3dRenderContext;
class Object3dCommon;
namespace Engine { class Camera; }

// LevelLoaderが読んだ通常3Dモデルを、ゲーム中のObject3dとして管理するクラスです。
// Player、Camera、鏡などのゲーム固有処理は持たず、Stage番号に関係なく共通利用できます。
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

	// モデル名から完成済みObjectを返す、呼出し元の生成関数です。
	using CreateObjectFunction = std::function<std::unique_ptr<Object3d>(const std::string& modelName)>;
	// @brief 呼出し元のFactory関数で通常3D配置物を仮生成し、全件成功後だけ採用します。
	// @param objectDataList JSON・CSVから分類された配置データです。nullptrの要素は失敗扱いです。
	// @param createObject モデル名から完成済みObjectを作る有効な関数です。
	// @return 全件を生成できた時だけtrueです。失敗時は旧配置を保持します。
	bool Rebuild(
		const std::vector<const LevelLoader::ObjectData*>& objectDataList,
		const CreateObjectFunction& createObject);
	// @brief 共通描画設定を借り、Object3dFactoryで全モデルを仮生成してから採用します。
	// @param objectDataList JSON・CSVから分類された配置データです。
	// @param object3dCommon Factoryがモデルを描画可能にするための共通設定です。所有しません。
	// @return 共通設定があり、全件を生成できた時だけtrueです。失敗時は旧配置を保持します。
	bool Rebuild(
		const std::vector<const LevelLoader::ObjectData*>& objectDataList,
		Object3dCommon* object3dCommon);
	// @brief 既存モデルを作り直さず、Editorで変更したTransformなどだけを反映します。
	// @param objectDataList 実行中モデルのsourceNameと照合する配置データです。対応のないモデルは維持します。
	void ApplyEdits(const std::vector<const LevelLoader::ObjectData*>& objectDataList);
	// @brief control_pointsを持つモデルを動かし、移動後のColliderを同期します。
	// @param deltaTime 前回の更新からの経過秒です。SceneがGame View中だけ渡します。
	void UpdatePaths(float deltaTime);
	// @brief Sceneが一度作ったCamera・Light設定を、所有する全モデルへ反映します。
	// @param renderContext Sceneが同じフレームで組み立てた描画準備です。所有しません。
	void UpdateRenderObjects(const Object3dRenderContext& renderContext);
	// @brief Mirror反射Cameraまたは通常Cameraの行列を、所有する全モデルへ反映します。
	// @param camera 次に描画する視点です。Sceneが所有し、このクラスは保持しません。
	void UpdateCameraForDraw(Engine::Camera* camera);
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
