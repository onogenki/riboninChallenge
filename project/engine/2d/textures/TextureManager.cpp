#include "TextureManager.h"
#include "DirectXCommon.h"
#include "Logger.h"
#include <cassert>
#include <filesystem>

// Manager本体は関数内で所有し、登録Textureの寿命だけをFinalizeで区切ります。
TextureManager* TextureManager::GetInstance()
{
	static TextureManager instance;
	return &instance;
}

// 借りたManagerを保持し、画像ごとのSRV登録先を先に確保します。
void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
	assert(dxCommon);
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	// 最大SRV数を予約しても画像は登録せず、読込成功時だけ一覧へ追加します。
	textureDatas_.reserve(DirectXCommon::kMaxSRVCount);
}

// 所有するSRVを返してから登録を消し、次の初期化へ古い参照を残しません。
void TextureManager::Finalize()
{
	if (srvManager_) {
		for (const auto& [filePath, textureData] : textureDatas_) {
			srvManager_->Free(textureData.srvIndex_);
		}
	}
	textureDatas_.clear();
	// 借りたManagerへの参照を残さず、再初期化前の誤使用を防ぎます。
	dxCommon_ = nullptr;
	srvManager_ = nullptr;
}

// 読込とSRV作成に成功した画像だけを登録し、失敗を空の登録で隠しません。
bool TextureManager::LoadTexture(const std::string& filePath)
{
	//読み込みテクスチャを検索
	if (textureDatas_.contains(filePath))
	{
		// 読み込み済みなら、SRV番号を変えずにそのまま使います。
		return true;
	}

	TextureData textureData{};
	if (!CreateTextureData(filePath, textureData, true)) {
		return false;
	}
	textureDatas_.emplace(filePath, std::move(textureData));
	return true;
}

// 保存時刻が変わった画像だけを再読込し、通常フレームのGPU転送を避けます。
void TextureManager::UpdateHotReload()
{
	for (auto& [filePath, textureData] : textureDatas_) {
		std::error_code error;
		const std::filesystem::file_time_type currentWriteTime =
			std::filesystem::last_write_time(filePath, error);
		if (error || !textureData.hasWriteTime_ || currentWriteTime == textureData.lastWriteTime_) {
			continue;
		}

		ReloadTexture(filePath, textureData);
	}
}

// 編集直後の確認用に、読込済みTextureを強制的に読み直します。
void TextureManager::ReloadAllLoadedTextures()
{
	for (auto& [filePath, textureData] : textureDatas_) {
		ReloadTexture(filePath, textureData);
	}
}

// 参照元が保持するSRV番号を変えないため、同じ番号の一時データを作ってから差し替えます。
void TextureManager::ReloadTexture(const std::string& filePath, TextureData& textureData)
{
	TextureData reloadedTexture{};
	reloadedTexture.srvIndex_ = textureData.srvIndex_;
	reloadedTexture.srvHandleCPU_ = textureData.srvHandleCPU_;
	reloadedTexture.srvHandleGPU_ = textureData.srvHandleGPU_;
	if (CreateTextureData(filePath, reloadedTexture, false)) {
		textureData = std::move(reloadedTexture);
	}
}

// 画像形式ごとの読込・GPU転送・SRV作成を一箇所に保ち、初回と再読込で同じ手順を使います。
bool TextureManager::CreateTextureData(
	const std::string& filePath,
	TextureData& textureData,
	bool allocateSrv)
{
	// テクスチャ枚数上限は、新しいSRV番号が必要な初回読込だけ確認します。
	if (!dxCommon_ || !srvManager_ || (allocateSrv && !srvManager_->CanAllocate())) {
		return false;
	}

	//テクスチャを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring wFilePath = std::wstring(filePath.begin(), filePath.end());
	HRESULT hr = S_OK;

	//拡張子による読み込み関数の分岐
	if (wFilePath.ends_with(L".dds"))//.ddsで終わっていたらddsとみなす。より安全な方法はいくらでもあるので余裕があれば対応するとよい
	{
		hr = DirectX::LoadFromDDSFile(wFilePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
	} else
	{
		hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	}
	if (FAILED(hr)) {
		Logger::Log("TextureManager: image file load failed: " + filePath);
		return false;
	}

	//ミップマップ生成の分岐
	DirectX::ScratchImage mipImages{};
	if (DirectX::IsCompressed(image.GetMetadata().format))//圧縮フォーマットかどうかを調べる
	{
		mipImages = std::move(image);//圧縮フォーマットならそのまま使うのでmoveする
	} else
	{//非圧縮フォーマットならMipMapを作成する
		hr = DirectX::GenerateMipMaps(
			image.GetImages(),
			image.GetImageCount(),
			image.GetMetadata(),
			DirectX::TEX_FILTER_SRGB,
			0, // 0を指定すると、1x1まで全てのミップマップを自動計算してくれます
			mipImages
		); 
		if (FAILED(hr)) {
			Logger::Log("TextureManager: mipmap generation failed: " + filePath);
			return false;
		}
	}

	textureData.filePath_ = filePath; //ファイルパス
	textureData.metadata_ = mipImages.GetMetadata(); //テクスチャメタデータを取得
	textureData.resource_ = dxCommon_->CreateTextureResource(textureData.metadata_);
	if (!textureData.resource_) {
		Logger::Log("TextureManager: texture resource creation failed: " + filePath);
		return false;
	}

	// 初回だけSRVを確保し、再読込は同じSRV番号を書き換えます。
	if (allocateSrv) {
		textureData.srvIndex_ = srvManager_->Allocate();
		if (textureData.srvIndex_ == SrvManager::kInvalidSrvIndex) {
			return false;
		}
		textureData.srvHandleCPU_ = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex_);
		textureData.srvHandleGPU_ = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex_);
	}

	//SRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metadata_.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (textureData.metadata_.IsCubemap()) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = UINT_MAX;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} else
	{//2DTextureの設定
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(textureData.metadata_.mipLevels);
	}

	// 先に転送とGPU完了待ちを終え、再読込時に古い画像を読む描画が残らない状態にします。
	// 同じSRV番号の差し替えを最後に行えば、追加のGPU待ちは必要ありません。
	if (!dxCommon_->ExecuteTextureTransfer(textureData.resource_, mipImages)) {
		Logger::Log("TextureManager: texture transfer failed: " + filePath);
		// 初回読込だけで取得したSRV番号は、空のTextureを登録せず返却します。
		if (allocateSrv) {
			srvManager_->Free(textureData.srvIndex_);
			textureData.srvIndex_ = SrvManager::kInvalidSrvIndex;
			textureData.srvHandleCPU_ = {};
			textureData.srvHandleGPU_ = {};
		}
		return false;
	}
	//設定をもとにSRVの生成
	dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource_.Get(), &srvDesc, textureData.srvHandleCPU_);
	std::error_code error;
	textureData.lastWriteTime_ = std::filesystem::last_write_time(filePath, error);
	textureData.hasWriteTime_ = !error;
	return true;
}

// 登録済み画像の寸法・形式を返し、取得だけで空の登録を作りません。
const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
	//読み込み済みかチェック
	assert(textureDatas_.contains(filePath));
	// 取得時に空の登録を作ると、後のLoadTextureが「読込済み」と誤認するためatで参照します。
	return textureDatas_.at(filePath).metadata_;
}

// 登録済み画像のSRV番号を返し、読込失敗を有効な番号として扱いません。
uint32_t TextureManager::GetSrvIndex(const std::string& filePath)
{
	// 読み込み済みかチェック
	assert(textureDatas_.contains(filePath));
	return textureDatas_.at(filePath).srvIndex_;
}

// 登録済み画像のGPUハンドルだけを返し、空のTextureを暗黙に追加しません。
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
{
	// 読み込み済みかチェック
	assert(textureDatas_.contains(filePath));
	return textureDatas_.at(filePath).srvHandleGPU_;
}
