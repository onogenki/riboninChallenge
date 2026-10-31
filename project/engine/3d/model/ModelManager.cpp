#include "ModelManager.h"

#include <filesystem>

namespace Engine
{

// 所有者を関数内の静的変数に限定し、呼び出し元には従来どおりポインタを貸します。
ModelManager* ModelManager::GetInstance()
{
	static ModelManager instance;
	return &instance;
}

// 共通DirectX操作窓口を先に所有し、後から作るModelへ有効な参照を渡します。
void ModelManager::Initialize(Engine::DirectXCommon* dxCommon)
{
	// Model共通部はManagerが所有し、モデルにはその参照を渡します。
	modelCommon_ = std::make_unique<ModelCommon>();
	modelCommon_->Initialize(dxCommon);
}

// Modelが借りる共通部を最後に解放し、参照先の寿命を守ります。
void ModelManager::Finalize()
{
	// Modelが借りるModelCommonを残したまま、先に各Modelを解放します。
	models_.clear();
	modelWriteTimes_.clear();
	modelCommon_.reset();
}

// 読込済みModelを再利用し、初回だけ生成して成功後に一覧へ登録します。
bool ModelManager::LoadModel(const std::string& fileName)
{
	if (models_.contains(fileName))
	{//読み込み済みなら早期return
		return true;
	}

	//モデルと生成とファイル読み込み、初期化
	std::unique_ptr<Model> model = std::make_unique<Model>();
	if (!model->Initialize(modelCommon_.get(), "resources", fileName)) {
		return false;
	}

	//モデルをmapコンテナに格納する
	models_.insert(std::make_pair(fileName, std::move(model)));
	std::error_code error;
	const std::filesystem::path modelPath = std::filesystem::path("resources") / fileName;
	const std::filesystem::file_time_type writeTime =
		std::filesystem::last_write_time(modelPath, error);
	if (!error) {
		modelWriteTimes_[fileName] = writeTime;
	}
	return true;

}

// 保存時刻が変わったModelだけを読み直し、Object3dが持つModelの住所を保ちます。
void ModelManager::UpdateHotReload()
{
	if (!modelCommon_) {
		return;
	}

	for (auto& [fileName, model] : models_) {
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
	if (!modelCommon_) {
		return;
	}

	for (auto& [fileName, model] : models_) {
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

// 読込成功後、古い頂点・材質をGPUが読み終えてから同じModelへ中身を移します。
bool ModelManager::ReloadModel(const std::string& fileName, Model& model)
{
	Model reloadedModel{};
	if (!reloadedModel.Initialize(modelCommon_.get(), "resources", fileName)) {
		return false;
	}
	if (!reloadedModel.CopyRuntimeSettingsFrom(model)) {
		return false;
	}
	// Object3dが持つModelのアドレスは維持し、差し替え前のGPU資源だけを安全に解放します。
	modelCommon_->GetDxCommon()->WaitForGPU();
	model = std::move(reloadedModel);
	return true;
}

// 未登録名では空の項目を作らずnullptrを返し、所有中のModelだけを貸します。
Model* ModelManager::FindModel(const std::string& fileName)
{//読み込み済みならモデルを検索
	if (models_.contains(fileName))
	{//読み込みモデルを戻り値としてreturn
		return models_.at(fileName).get();
	}
	//ファイル名一致なし
	return nullptr;
}

ModelManager::~ModelManager()
{
	// 通常終了以外でも、Modelから共通部への参照が有効な順で片付けます。
	Finalize();
}

}
