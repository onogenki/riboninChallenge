#pragma once
#include <string>
#include "Camera.h"
#include <unordered_map>
class CameraManager
{

public:

	//カメラ登録
	void AddCamera(const std::string& name, Camera* camera);
	//登録名に一致するカメラを管理対象から外す
	void RemoveCamera(const std::string& name);
	//使うカメラを切り替える
	void SetActiveCamera(const std::string& name);
	Camera* GetActiveCamera() { return activeCamera_; }
	void Update();

	CameraManager() = default;
	// デストラクタを追加
	~CameraManager();
private:
	//文字列とカメラをセットで保存
	std::unordered_map<std::string, Camera*> cameras_;
	Camera* activeCamera_ = nullptr;

};

