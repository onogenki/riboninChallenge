#include "ModelManager.h"

#include <filesystem>

// 所有者を関数内の静的変数に限定し、呼び出し元には従来どおりポインタを貸します。
ModelManager* ModelManager::GetInstance()
{
	static ModelManager instance;
	return &instance;
}

void ModelManager::Initialize(DirectXCommon* dxCommon)
{
	// Model共通部はManagerが所有し、モデルにはその参照を渡します。
	modelCommon = std::make_unique<ModelCommon>();
	modelCommon->Initialize(dxCommon);
}

void ModelManager::Finalize()
{
	// モデルが借りる共通部を先に解放する従来の順序を保ちます。
	modelCommon.reset();
	models.clear();
	modelWriteTimes_.clear();
}

bool ModelManager::LoadModel(const std::string& fileName)
{
	if (models.contains(fileName))
	{//読み込み済みなら早期return
		return true;
	}

	//モデルと生成とファイル読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	if (!model->Initialize(modelCommon.get(), "resources", fileName)) {
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
		if (ReloadModel(fileName, *model)) {
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
		if (ReloadModel(fileName, *model)) {
			std::error_code error;
			modelWriteTimes_[fileName] = std::filesystem::last_write_time(
				std::filesystem::path("resources") / fileName, error);
		}
	}
}

// Object3dがModelのアドレスを保持するため、読込成功後に同じインスタンスへ移します。
bool ModelManager::ReloadModel(const std::string& fileName, Model& model)
{
	Model reloadedModel{};
	if (!reloadedModel.Initialize(modelCommon.get(), "resources", fileName)) {
		return false;
	}
	model = std::move(reloadedModel);
	return true;
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
	// 従来どおり、モデル格納コンテナより先に共通部を解放します。
	modelCommon.reset();
	// map内のunique_ptrは自動的に解放されるため、明示的なdeleteは不要
}
