#pragma once
#include "Model.h"
#include "ModelCommon.h"
#include "DirectXCommon.h"
#include <map>
#include <string>
#include <memory>
#include <filesystem>
//テクスチャマネージャー
class ModelManager
{
public:
	//シングルインスタンスの取得
	static ModelManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon);

	//終了
	void Finalize();

	//モデルファイルの読み込み
	bool LoadModel(const std::string& fileName);
	// 保存時刻が変わったモデルだけを、Object3dが持つポインタを保ったまま再読込します。
	void UpdateHotReload();
	// 保存時刻に関係なく、現在読み込み済みの全モデルを再読込します。
	void ReloadAllLoadedModels();

	//モデルファイルパス
	Model* FindModel(const std::string& fileName);

private:
	static ModelManager* instance;

	ModelManager() = default;
	~ModelManager();
	ModelManager(const ModelManager&) = delete;
	ModelManager& operator =(const ModelManager&) = delete;

	ModelCommon* modelCommon = nullptr;

	//モデルデータ
	std::map<std::string, std::unique_ptr<Model>>models;
	// モデルごとの前回読込時刻です。存在しない時は監視対象にしません。
	std::map<std::string, std::filesystem::file_time_type> modelWriteTimes_;
};

