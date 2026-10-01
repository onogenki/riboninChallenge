#include "StageCameraEvents.h"

#include "Camera.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "Player.h"
#include <algorithm>
#include <cmath>
#include <utility>

using namespace MyMath;

StageCameraEvents::~StageCameraEvents() = default;

namespace
{
	void ApplyBoxCollider(
		Transform& transform,
		ObbCollider& collider,
		const LevelLoader::ObjectData& objectData)
	{
		transform = {
			objectData.scaling,
			objectData.rotation,
			objectData.translation,
		};
		Vector3 localCenter{};
		Vector3 localHalfSize{ 1.0f, 1.0f, 1.0f };
		if (objectData.hasCollider && objectData.collider.type == "BOX") {
			localCenter = objectData.collider.center;
			localHalfSize = {
				std::abs(objectData.collider.size.x) * 0.5f,
				std::abs(objectData.collider.size.y) * 0.5f,
				std::abs(objectData.collider.size.z) * 0.5f,
			};
		}
		collider.SetLocalShape(localCenter, localHalfSize);
		collider.SyncTransform(transform);
	}
}

void StageCameraEvents::Rebuild(
	const std::vector<const LevelLoader::ObjectData*>& triggerDataList,
	const std::vector<const LevelLoader::ObjectData*>& cameraDataList,
	const std::vector<const LevelLoader::ObjectData*>& cameraAreaDataList,
	CameraManager* cameraManager,
	CameraController* mainCameraController)
{
	Clear(cameraManager);

	eventTriggers_.reserve(triggerDataList.size());
	for (const LevelLoader::ObjectData* objectData : triggerDataList) {
		if (!objectData) {
			continue;
		}
		EventTrigger eventTrigger{};
		eventTrigger.sourceName = objectData->name;
		eventTrigger.eventId = objectData->eventId;
		eventTrigger.eventCameraName = objectData->eventCameraName;
		ApplyBoxCollider(
			eventTrigger.transform,
			eventTrigger.collider,
			*objectData);
		eventTriggers_.push_back(std::move(eventTrigger));
	}

	eventCameras_.reserve(cameraDataList.size());
	for (const LevelLoader::ObjectData* objectData : cameraDataList) {
		if (!objectData) {
			continue;
		}
		EventCamera eventCamera{};
		eventCamera.sourceName = objectData->name;
		eventCamera.camera = std::make_unique<Camera>();
		ApplyEventCameraData(eventCamera, *objectData);
		eventCameras_.push_back(std::move(eventCamera));
	}

	cameraAreas_.reserve(cameraAreaDataList.size());
	for (const LevelLoader::ObjectData* objectData : cameraAreaDataList) {
		if (!objectData) {
			continue;
		}
		CameraArea cameraArea{};
		cameraArea.sourceName = objectData->name;
		ApplyBoxCollider(
			cameraArea.transform,
			cameraArea.collider,
			*objectData);
		cameraArea.settings = {
			objectData->cameraArea.distance,
			objectData->cameraArea.pitch,
			objectData->cameraArea.fovY,
		};
		cameraAreas_.push_back(std::move(cameraArea));
	}

	if (mainCameraController) {
		mainCameraController->ClearAreaSettings();
	}
	if (cameraManager) {
		for (const EventCamera& eventCamera : eventCameras_) {
			cameraManager->AddCamera(eventCamera.sourceName, eventCamera.camera.get());
		}
	}
}

void StageCameraEvents::ApplyEdits(
	const std::vector<const LevelLoader::ObjectData*>& triggerDataList,
	const std::vector<const LevelLoader::ObjectData*>& cameraDataList,
	const std::vector<const LevelLoader::ObjectData*>& cameraAreaDataList)
{
	for (EventTrigger& eventTrigger : eventTriggers_) {
		const auto found = std::find_if(
			triggerDataList.begin(), triggerDataList.end(),
			[&](const LevelLoader::ObjectData* objectData)
			{
				return objectData && objectData->name == eventTrigger.sourceName;
			});
		if (found == triggerDataList.end()) {
			continue;
		}
		const LevelLoader::ObjectData& objectData = **found;
		eventTrigger.eventId = objectData.eventId;
		eventTrigger.eventCameraName = objectData.eventCameraName;
		ApplyBoxCollider(
			eventTrigger.transform,
			eventTrigger.collider,
			objectData);
	}

	for (EventCamera& eventCamera : eventCameras_) {
		const auto found = std::find_if(
			cameraDataList.begin(), cameraDataList.end(),
			[&](const LevelLoader::ObjectData* objectData)
			{
				return objectData && objectData->name == eventCamera.sourceName;
			});
		if (found != cameraDataList.end()) {
			ApplyEventCameraData(eventCamera, **found);
		}
	}

	for (CameraArea& cameraArea : cameraAreas_) {
		const auto found = std::find_if(
			cameraAreaDataList.begin(), cameraAreaDataList.end(),
			[&](const LevelLoader::ObjectData* objectData)
			{
				return objectData && objectData->name == cameraArea.sourceName;
			});
		if (found == cameraAreaDataList.end()) {
			continue;
		}
		const LevelLoader::ObjectData& objectData = **found;
		ApplyBoxCollider(
			cameraArea.transform,
			cameraArea.collider,
			objectData);
		cameraArea.settings = {
			objectData.cameraArea.distance,
			objectData.cameraArea.pitch,
			objectData.cameraArea.fovY,
		};
	}
}

void StageCameraEvents::UpdateEventCamera(
	Player& player,
	CameraManager& cameraManager,
	const std::vector<OBB>& solidObbs,
	float deltaTime)
{
	std::string requestedCameraName;
	for (EventTrigger& eventTrigger : eventTriggers_) {
		eventTrigger.isPlayerInside = player.CheckCollision(eventTrigger.collider).isCollision;
		if (!eventTrigger.isPlayerInside || !requestedCameraName.empty()) {
			continue;
		}

		const auto cameraFound = std::find_if(
			eventCameras_.begin(), eventCameras_.end(),
			[&](const EventCamera& eventCamera)
			{
				return eventCamera.sourceName == eventTrigger.eventCameraName;
			});
		if (cameraFound != eventCameras_.end()) {
			requestedCameraName = cameraFound->sourceName;
		}
	}

	if (!requestedCameraName.empty() && activeEventCameraName_ != requestedCameraName) {
		auto cameraFound = std::find_if(
			eventCameras_.begin(), eventCameras_.end(),
			[&](const EventCamera& eventCamera)
			{
				return eventCamera.sourceName == requestedCameraName;
			});
		if (cameraFound != eventCameras_.end() && cameraFound->camera) {
			const Vector3 initialFocus{
				player.GetPosition().x,
				player.GetPosition().y + 1.0f,
				player.GetPosition().z,
			};
			const Vector3 cameraOffset{
				cameraFound->camera->GetTranslate().x - initialFocus.x,
				cameraFound->camera->GetTranslate().y - initialFocus.y,
				cameraFound->camera->GetTranslate().z - initialFocus.z,
			};
			const float distance = (std::max)(Length(cameraOffset), 3.0f);
			const float yaw = std::atan2(-cameraOffset.x, -cameraOffset.z);
			const float pitch = std::asin(std::clamp(cameraOffset.y / distance, -1.0f, 1.0f));
			cameraFound->manualController = std::make_unique<CameraController>();
			cameraFound->manualController->Initialize(
				cameraFound->camera.get(), player.GetPosition(), distance, yaw, pitch);
			cameraFound->manualController->SetAutoRecenterEnabled(false);
			cameraManager.SetActiveCamera(requestedCameraName);
			activeEventCameraName_ = requestedCameraName;
			status_ = "Event Manual Camera active: " + requestedCameraName;
		}
	} else if (requestedCameraName.empty() && !activeEventCameraName_.empty()) {
		RestoreMainCamera(cameraManager);
		status_ = "Event finished. MainCamera restored.";
	}

	if (activeEventCameraName_.empty()) {
		return;
	}
	const auto activeCameraFound = std::find_if(
		eventCameras_.begin(), eventCameras_.end(),
		[this](const EventCamera& eventCamera)
		{
			return eventCamera.sourceName == activeEventCameraName_;
		});
	if (activeCameraFound == eventCameras_.end() || !activeCameraFound->manualController) {
		return;
	}

	Input* input = Input::GetInstance();
	const bool isOrbitInput = input->IsMouseButtonPressed(1);
	if (isOrbitInput) {
		activeCameraFound->manualController->AddOrbitYaw(
			-static_cast<float>(input->GetMouseX()) * 0.005f);
		activeCameraFound->manualController->AddOrbitPitch(
			-static_cast<float>(input->GetMouseY()) * 0.003f);
	}
	const float distance = std::clamp(
		activeCameraFound->manualController->GetDistance() -
			static_cast<float>(input->GetMouseWheel()) * 0.005f,
		3.0f,
		20.0f);
	activeCameraFound->manualController->SetDistance(distance);
	activeCameraFound->manualController->Update(
		deltaTime,
		player.GetPosition(),
		player.GetMoveDirection(),
		isOrbitInput,
		solidObbs);
}

void StageCameraEvents::UpdateCameraArea(Player& player, CameraController& mainCameraController)
{
	CameraArea* requestedArea = nullptr;
	for (CameraArea& cameraArea : cameraAreas_) {
		cameraArea.isPlayerInside = player.CheckCollision(cameraArea.collider).isCollision;
		if (cameraArea.isPlayerInside && !requestedArea) {
			requestedArea = &cameraArea;
		}
	}

	if (requestedArea && activeCameraAreaName_ != requestedArea->sourceName) {
		mainCameraController.SetAreaSettings(requestedArea->settings);
		activeCameraAreaName_ = requestedArea->sourceName;
		status_ = "Camera Area active: " + activeCameraAreaName_;
	} else if (!requestedArea && !activeCameraAreaName_.empty()) {
		mainCameraController.ClearAreaSettings();
		activeCameraAreaName_.clear();
		status_ = "Camera Area finished. Automatic camera restored.";
	}
}

std::string StageCameraEvents::ConsumeStatus()
{
	return std::exchange(status_, {});
}

void StageCameraEvents::RestoreMainCamera(CameraManager& cameraManager)
{
	for (EventCamera& eventCamera : eventCameras_) {
		if (eventCamera.sourceName == activeEventCameraName_) {
			eventCamera.manualController.reset();
			break;
		}
	}
	cameraManager.SetActiveCamera("MainCamera");
	activeEventCameraName_.clear();
}

void StageCameraEvents::Clear(CameraManager* cameraManager)
{
	if (cameraManager) {
		cameraManager->SetActiveCamera("MainCamera");
		for (const EventCamera& eventCamera : eventCameras_) {
			cameraManager->RemoveCamera(eventCamera.sourceName);
		}
	}
	eventCameras_.clear();
	eventTriggers_.clear();
	cameraAreas_.clear();
	activeEventCameraName_.clear();
	activeCameraAreaName_.clear();
	status_.clear();
}

void StageCameraEvents::ApplyEventCameraData(
	EventCamera& eventCamera,
	const LevelLoader::ObjectData& objectData)
{
	if (!eventCamera.camera) {
		return;
	}
	eventCamera.camera->SetTranslate(objectData.translation);
	eventCamera.hasFocus = objectData.hasCameraFocus;
	eventCamera.focus = objectData.cameraFocus;
	if (!eventCamera.hasFocus) {
		eventCamera.camera->SetRotate(objectData.rotation);
		return;
	}

	const Vector3 lookDirection = Normalize({
		eventCamera.focus.x - objectData.translation.x,
		eventCamera.focus.y - objectData.translation.y,
		eventCamera.focus.z - objectData.translation.z,
	});
	const float pitch = -std::asin(std::clamp(lookDirection.y, -1.0f, 1.0f));
	const float yaw = std::atan2(lookDirection.x, lookDirection.z);
	eventCamera.camera->SetRotate({ pitch, yaw, 0.0f });
}
