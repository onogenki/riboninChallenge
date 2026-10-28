#include "CameraManager.h"

namespace Engine
{
void CameraManager::AddCamera(const std::string& name, Engine::Camera* camera)
{
	if (!camera) {
		return;
	}

	cameras_[name] = camera;
	// ポインタが同じ別名を差し替えても、選択中の名前が変わらないよう名前で判定します。
	if (activeCamera_ == nullptr || activeCameraName_ == name) {
		activeCameraName_ = name;
		activeCamera_ = camera;
	}
}

void CameraManager::RemoveCamera(const std::string& name)
{
	const auto found = cameras_.find(name);
	if (found == cameras_.end()) {
		return;
	}

	// 別名が同じCameraを指していても、選択中の登録を外す場合だけ無効にします。
	if (activeCamera_ != nullptr && activeCameraName_ == name) {
		activeCameraName_.clear();
		activeCamera_ = nullptr;
	}
	cameras_.erase(found);
}

void CameraManager::SetActiveCamera(const std::string& name)
{
	// 未登録名では現在の選択を保ち、見つかった名前とポインタを一組で更新します。
	const auto it = cameras_.find(name);
	if (it != cameras_.end()) {
		activeCameraName_ = name;
		activeCamera_ = it->second;
	}
}

void CameraManager::Update()
{
	//全てのカメラを順番に取り出す
	for (auto& pair : cameras_)
	{
		if (pair.second) {
			pair.second->Update();
		}
	}
}

CameraManager::~CameraManager()
{
	// Camera本体はSceneに残し、非所有の登録と選択だけを外します。
	cameras_.clear();
	activeCameraName_.clear();
	activeCamera_ = nullptr;
}
}
