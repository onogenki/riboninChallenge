#include "CameraManager.h"

void CameraManager::AddCamera(const std::string& name, Camera* camera)
{
	if (!camera) {
		return;
	}

	//カメラを書き込む
	cameras_[name] = camera;
	//現在使用中のカメラが決まってなければ
	if (activeCamera_ == nullptr)
	{//とりあえずメインカメラに設定
		activeCamera_ = camera;
	}

}

void CameraManager::RemoveCamera(const std::string& name)
{
	const auto found = cameras_.find(name);
	if (found == cameras_.end()) {
		return;
	}

	// 使用中のCameraを削除する場合は、無効なポインタを残さない
	if (activeCamera_ == found->second) {
		activeCamera_ = nullptr;
	}
	cameras_.erase(found);
}

void CameraManager::SetActiveCamera(const std::string& name)
{
	//カメラの中から指定された名前を探す
	auto it = cameras_.find(name);
	
	//もし見つからなかったら
	if (it != cameras_.end())
	{//見つかったカメラを今のアクティブカメラに設定
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
	// コンテナの中身も空にしておく（安全のため）
	cameras_.clear();
}
