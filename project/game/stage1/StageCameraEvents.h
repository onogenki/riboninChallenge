#pragma once

#include "CameraController.h"
#include "Collider.h"
#include "LevelLoader.h"
#include <memory>
#include <string>
#include <vector>

class Camera;
class CameraManager;
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

	// Event中だけCameraManagerへ登録するCameraと、その操作役です。
	struct EventCamera
	{
		std::string sourceName;
		std::unique_ptr<Camera> camera;
		std::unique_ptr<CameraController> manualController;
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
		CameraAreaSettings settings{};
		bool isPlayerInside = false;
	};

	// JSON読込後、EventとAreaを作り直してCameraManagerへ登録します。
	void Rebuild(
		const std::vector<const LevelLoader::ObjectData*>& triggerDataList,
		const std::vector<const LevelLoader::ObjectData*>& cameraDataList,
		const std::vector<const LevelLoader::ObjectData*>& cameraAreaDataList,
		CameraManager* cameraManager,
		CameraController* mainCameraController);
	// Editorで変更したTransformやFocusを、実行中のEventとAreaへ反映します。
	void ApplyEdits(
		const std::vector<const LevelLoader::ObjectData*>& triggerDataList,
		const std::vector<const LevelLoader::ObjectData*>& cameraDataList,
		const std::vector<const LevelLoader::ObjectData*>& cameraAreaDataList);
	// PlayerがTriggerへ入った時のCamera切替と、Event Cameraの手動操作を更新します。
	void UpdateEventCamera(
		Player& player,
		CameraManager& cameraManager,
		const std::vector<MyMath::OBB>& solidObbs,
		float deltaTime);
	// PlayerがCamera Areaへ入った時の通常Camera設定を更新します。
	void UpdateCameraArea(Player& player, CameraController& mainCameraController);
	// Event中でなければ空文字、状態が切り替わった時は説明文を一度だけ返します。
	std::string ConsumeStatus();
	// Event Cameraを解除し、MainCameraへ戻します。
	void RestoreMainCamera(CameraManager& cameraManager);
	// CameraManagerからEvent Cameraを外し、所有データを解放します。
	void Clear(CameraManager* cameraManager);

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
