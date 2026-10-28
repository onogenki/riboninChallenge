#pragma once

#include "Camera.h"
#include <string>
#include <unordered_map>

namespace Engine
{
// Sceneが所有するCameraを名前で登録し、描画に使うCameraを選びます。
// 登録ポインタの所有権は受け取らず、Camera本体はScene側が管理します。
class CameraManager
{
public:
	// nameにCameraを登録します。cameraがnullptrなら何もせず、最初の登録を有効Cameraにします。
	// 同じnameを使うと登録を置き換え、使用中のnameなら有効Cameraも新しい方へ切り替えます。
	void AddCamera(const std::string& name, Engine::Camera* camera);
	// nameの登録を外します。有効Cameraを外した場合は、再選択するまでnullptrにします。
	void RemoveCamera(const std::string& name);
	// nameに一致するCameraへ切り替えます。未登録なら現在の選択を維持します。
	void SetActiveCamera(const std::string& name);
	// 有効Cameraを返します。未登録または解除後はnullptrなので利用側で確認します。
	Engine::Camera* GetActiveCamera() { return activeCamera_; }
	// 登録済みのCameraを更新します。unordered_mapのため更新順には依存しません。
	void Update();

	CameraManager() = default;
	// Camera本体は破棄せず、非所有の登録だけを解放します。
	~CameraManager();

private:
	// Cameraの寿命はSceneが管理するため、ここには非所有ポインタを保持します。
	std::unordered_map<std::string, Engine::Camera*> cameras_;
	// 同じCameraを別名で登録しても、選択中の登録を名前で区別します。
	std::string activeCameraName_;
	Engine::Camera* activeCamera_ = nullptr;
};
}

