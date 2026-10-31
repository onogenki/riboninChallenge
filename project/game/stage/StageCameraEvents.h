#pragma once

#include "CameraController.h"
#include "Collider.h"
#include "LevelLoader.h"
#include <memory>
#include <string>
#include <vector>

namespace Engine { class Camera; }
namespace Engine { class CameraManager; }
class Player;

// JSONのEVENT_TRIGGER・EVENT_CAMERA・CAMERA_AREAを実行するCamera専用クラスです。
// Stage固有の鏡やLightを持たないため、ほかのStageでも同じ書式のJSONを利用できます。
class StageCameraEvents
{
public:
	~StageCameraEvents();

	// Playerが入るとEvent Cameraを起動する、見えないBOXです。
	struct EventTrigger
	{
		std::string sourceName;
		std::string eventId;
		std::string eventCameraName;
		Transform transform{};
		// 見えないTriggerの位置・回転・大きさを持つ箱Colliderです。
		ObbCollider collider{};
		bool isPlayerInside = false;
	};

	// 再構築時にCameraManagerへ登録され、Event中に選択されるCameraと、その操作役です。
	struct EventCamera
	{
		std::string sourceName;
		std::unique_ptr<Engine::Camera> camera;
		std::unique_ptr<Engine::CameraController> manualController;
		Vector3 focus{};
		bool hasFocus = false;
	};

	// 通常Cameraの距離・角度・画角を一時的に切り替える、見えないBOXです。
	struct CameraArea
	{
		std::string sourceName;
		Transform transform{};
		// 見えないCamera Areaの位置・回転・大きさを持つ箱Colliderです。
		ObbCollider collider{};
		Engine::CameraAreaSettings settings{};
		bool isPlayerInside = false;
	};

	// JSON読込後に旧登録を外し、EventとAreaを作り直してCameraManagerへ登録します。
	// 各Data一覧は呼び出し中だけ参照し、CameraManagerと通常CameraのControllerは所有しません。
	// cameraManagerとmainCameraControllerがnullptrなら、その相手への登録・設定解除を省きます。
	// 旧登録を外した後に新しいCameraを生成するため、途中で例外が発生した場合の旧状態復元は行いません。
	void Rebuild(
		const std::vector<const LevelLoader::ObjectData*>& triggerDataList,
		const std::vector<const LevelLoader::ObjectData*>& cameraDataList,
		const std::vector<const LevelLoader::ObjectData*>& cameraAreaDataList,
		Engine::CameraManager* cameraManager,
		Engine::CameraController* mainCameraController);
	// Editorで変更した同名ObjectのTransformやFocusを、生成済みEventとAreaへ反映します。
	// 新規追加・削除の反映にはRebuildを使います。Data一覧は呼び出し中だけ参照します。
	void ApplyEdits(
		const std::vector<const LevelLoader::ObjectData*>& triggerDataList,
		const std::vector<const LevelLoader::ObjectData*>& cameraDataList,
		const std::vector<const LevelLoader::ObjectData*>& cameraAreaDataList);
	// PlayerがTriggerへ入った時のCamera切替と、Event Cameraの手動操作を更新します。
	// solidObbsはCameraの壁回避に使い、deltaTimeはControllerの一フレーム分の経過秒です。
	void UpdateEventCamera(
		Player& player,
		Engine::CameraManager& cameraManager,
		const std::vector<MyMath::OBB>& solidObbs,
		float deltaTime);
	// PlayerがCamera Areaへ入った時の通常Camera設定を更新し、出た時に元へ戻します。
	void UpdateCameraArea(Player& player, Engine::CameraController& mainCameraController);
	// 直前に記録された状態変更の説明文を一度だけ返して消します。変更がなければ空文字です。
	std::string ConsumeStatus();
	// Event Cameraを解除し、MainCameraへ戻します。
	void RestoreMainCamera(Engine::CameraManager& cameraManager);
	// CameraManagerからEvent Cameraを外し、所有データを解放します。
	// cameraManagerがnullptrなら登録解除を省きます。Stage終了時は登録元を渡します。
	void Clear(Engine::CameraManager* cameraManager);

	const std::vector<EventTrigger>& GetTriggers() const { return eventTriggers_; }
	const std::vector<EventCamera>& GetEventCameras() const { return eventCameras_; }
	const std::string& GetActiveEventCameraName() const { return activeEventCameraName_; }

private:
	// JSONのEvent Camera位置と注視点から、Cameraの回転を計算します。
	static void ApplyEventCameraData(EventCamera& eventCamera, const LevelLoader::ObjectData& objectData);

	std::vector<EventTrigger> eventTriggers_;
	std::vector<EventCamera> eventCameras_;
	std::vector<CameraArea> cameraAreas_;
	std::string activeEventCameraName_;
	std::string activeCameraAreaName_;
	std::string status_;
};
