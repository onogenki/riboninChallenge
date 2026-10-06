#pragma once
#include "Model.h"
#include "ModelCommon.h"
#include "DirectXCommon.h"
#include <map>
#include <string>
#include <memory>
#include <filesystem>
// 読み込んだモデルを一元所有し、Object3dへ所有権のない参照を渡します。
class ModelManager
{
public:
	//シングルインスタンスの取得
	static ModelManager* GetInstance();

	// 初期化済みDirectXを借り、モデル読込が使う共通部を作ります。
	void Initialize(DirectXCommon* dxCommon);

	// Managerが所有するモデルと共通部を解放します。
	void Finalize();

	// resources以下のfileNameを読み、成功または読込済みならtrueを返します。
	bool LoadModel(const std::string& fileName);
	// 保存時刻が変わったモデルだけを、Object3dが持つポインタを保ったまま再読込します。
	void UpdateHotReload();
	// 保存時刻に関係なく、現在読み込み済みの全モデルを再読込します。
	void ReloadAllLoadedModels();

	// 登録済みのfileNameを探し、なければnullptrを返します。空の登録は作りません。
	Model* FindModel(const std::string& fileName);

private:
	ModelManager() = default;
	~ModelManager();
	ModelManager(const ModelManager&) = delete;
	ModelManager& operator =(const ModelManager&) = delete;

	// ModelManagerだけが所有し、Modelには生存中の参照だけを渡します。
	std::unique_ptr<ModelCommon> modelCommon;
	// 既存のModelのアドレスを保ったまま中身を読み直し、成功時だけ差し替えます。
	bool ReloadModel(const std::string& fileName, Model& model);

	//モデルデータ
	std::map<std::string, std::unique_ptr<Model>>models;
	// モデルごとの前回読込時刻です。存在しない時は監視対象にしません。
	std::map<std::string, std::filesystem::file_time_type> modelWriteTimes_;
};

