#pragma once
#include "Model.h"
#include "ModelCommon.h"
#include "DirectXCommon.h"
#include <map>
#include <string>
#include <memory>
#include <filesystem>

namespace Engine
{

// 読み込んだモデルを一元所有し、Object3dへ所有権のない参照を渡します。
class ModelManager
{
public:
	//シングルインスタンスの取得
	static ModelManager* GetInstance();

	// @brief 初期化済みDirectXを借り、モデル読込が使う共通部を作ります。
	// @param dxCommon ModelManagerのFinalizeまで生存するDirectX共通部です。
	void Initialize(DirectXCommon* dxCommon);

	// Managerが所有するモデルと共通部を解放します。
	void Finalize();

	// @brief resources以下のModelを読み、完成したものだけManagerへ登録します。
	// @param fileName resourcesを基点にしたモデルファイル名です。
	// @return 読込済み、または今回の初期化と登録に成功した時だけtrueです。
	bool LoadModel(const std::string& fileName);
	// 保存時刻が変わったモデルだけを、Object3dが持つポインタを保ったまま再読込します。
	void UpdateHotReload();
	// 保存時刻に関係なく、現在読み込み済みの全モデルを再読込します。
	void ReloadAllLoadedModels();

	// @brief 登録済みModelを貸します。取得だけでは空の登録を作りません。
	// @param fileName LoadModelで使用したモデルファイル名です。
	// @return 登録済みならManager所有のModel、未登録ならnullptrです。Finalize後は保持できません。
	Model* FindModel(const std::string& fileName);

private:
	ModelManager() = default;
	~ModelManager();
	ModelManager(const ModelManager&) = delete;
	ModelManager& operator =(const ModelManager&) = delete;

	// ModelManagerだけが所有し、Modelには生存中の参照だけを渡します。
	std::unique_ptr<ModelCommon> modelCommon_;
	// 既存のModelのアドレスを保ったまま中身を読み直し、成功時だけ差し替えます。
	bool ReloadModel(const std::string& fileName, Model& model);

	//モデルデータ
	std::map<std::string, std::unique_ptr<Model>> models_;
	// モデルごとの前回読込時刻です。存在しない時は監視対象にしません。
	std::map<std::string, std::filesystem::file_time_type> modelWriteTimes_;
};

}

