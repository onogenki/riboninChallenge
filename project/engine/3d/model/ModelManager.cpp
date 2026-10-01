#include "ModelManager.h"

#include <filesystem>

ModelManager* ModelManager::instance = nullptr;

ModelManager* ModelManager::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new ModelManager();
	}
	return instance;
}

void ModelManager::Initialize(DirectXCommon* dxCommon)
{
	modelCommon = new ModelCommon;
	modelCommon->Initialize(dxCommon);
}

void ModelManager::Finalize()
{
	delete instance;
	instance = nullptr;
}

bool ModelManager::LoadModel(const std::string& fileName)
{
	if (models.contains(fileName))
	{//読み込み済みなら早期return
		return true;
	}

	//モデルと生成とファイル読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	if (!model->Initialize(modelCommon, "resources", fileName)) {
		return false;
	}

	//モデルをmapコンテナに格納する
	models.insert(std::make_pair(fileName, std::move(model)));
	std::error_code error;
	const std::filesystem::path modelPath = std::filesystem::path("resources") / fileName;
	const std::filesystem::file_time_type writeTime =
		std::filesystem::last_write_time(modelPath, error);
	if (!error) {
		modelWriteTimes_[fileName] = writeTime;
	}
	return true;

}

void ModelManager::UpdateHotReload()
{
	if (!modelCommon) {
		return;
	}

	for (auto& [fileName, model] : models) {
		if (!model) {
			continue;
		}

		const std::filesystem::path modelPath = std::filesystem::path("resources") / fileName;
		std::error_code error;
		const std::filesystem::file_time_type currentWriteTime =
			std::filesystem::last_write_time(modelPath, error);
		if (error) {
			continue;
		}

		const auto previousWriteTime = modelWriteTimes_.find(fileName);
		if (previousWriteTime == modelWriteTimes_.end()) {
			modelWriteTimes_[fileName] = currentWriteTime;
			continue;
		}
		if (currentWriteTime == previousWriteTime->second) {
			continue;
		}

		// Object3dはModelのアドレスを保持しているため、unique_ptr自体は入れ替えません。
		// 新しいModelの中身だけを同じアドレスへ移し、既存の描画Objectからも反映されます。
		Model reloadedModel{};
		if (reloadedModel.Initialize(modelCommon, "resources", fileName)) {
			*model = std::move(reloadedModel);
			previousWriteTime->second = currentWriteTime;
		}
	}
}

// 編集直後の確認用に、読込済みモデルを強制的に読み直します。
void ModelManager::ReloadAllLoadedModels()
{
	if (!modelCommon) {
		return;
	}

	for (auto& [fileName, model] : models) {
		if (!model) {
			continue;
		}
		Model reloadedModel{};
		if (reloadedModel.Initialize(modelCommon, "resources", fileName)) {
			*model = std::move(reloadedModel);
			std::error_code error;
			modelWriteTimes_[fileName] = std::filesystem::last_write_time(
				std::filesystem::path("resources") / fileName, error);
		}
	}
}

Model* ModelManager::FindModel(const std::string& fileName)
{//読み込み済みならモデルを検索
	if (models.contains(fileName))
	{//読み込みモデルを戻り値としてreturn
		return models.at(fileName).get();
	}
	//ファイル名一致なし
	return nullptr;
}

ModelManager::~ModelManager()
{
	// モデル共通部の解放
	if (modelCommon) {
		delete modelCommon;
		modelCommon = nullptr;
	}
	// map内のunique_ptrは自動的に解放されるため、明示的なdeleteは不要
}
