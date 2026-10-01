#include "TextureManager.h"
#include "DirectXCommon.h"
#include <cassert>
#include <filesystem>

TextureManager* TextureManager::instance = nullptr;

//ImGuiで0番を使用するため、1番から使用
//uint32_t TextureManager::kSRVIndexTop = 1;

TextureManager* TextureManager::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new TextureManager;
	}
	return instance;
}

void TextureManager::Initialize(DirectXCommon* dxCommon,SrvManager* srvManager)
{//srvの数と同数
	assert(dxCommon);
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;
	textureDatas.reserve(DirectXCommon::kMaxSRVCount);
}

void TextureManager::Finalize()
{
	if (srvManager_) {
		for (const auto& [filePath, textureData] : textureDatas) {
			srvManager_->Free(textureData.srvIndex);
		}
	}
	textureDatas.clear();
	delete instance;
	instance = nullptr;
}

bool TextureManager::LoadTexture(const std::string& filePath)
{
	//読み込みテクスチャを検索
	if (textureDatas.contains(filePath))
	{
		//読み込み済みなら早朝return 
		return true;
	}

	TextureData textureData{};
	if (!CreateTextureData(filePath, textureData, true)) {
		return false;
	}
	textureDatas.emplace(filePath, std::move(textureData));
	return true;
}

void TextureManager::UpdateHotReload()
{
	for (auto& [filePath, textureData] : textureDatas) {
		std::error_code error;
		const std::filesystem::file_time_type currentWriteTime =
			std::filesystem::last_write_time(filePath, error);
		if (error || !textureData.hasWriteTime || currentWriteTime == textureData.lastWriteTime) {
			continue;
		}

		// 既存のSRV番号を引き継ぐため、SpriteやObject3dが持つ番号は変わりません。
		TextureData reloadedTexture{};
		reloadedTexture.srvIndex = textureData.srvIndex;
		reloadedTexture.srvHandleCPU = textureData.srvHandleCPU;
		reloadedTexture.srvHandleGPU = textureData.srvHandleGPU;
		if (CreateTextureData(filePath, reloadedTexture, false)) {
			textureData = std::move(reloadedTexture);
		}
	}
}

// 編集直後の確認用に、読込済みTextureを強制的に読み直します。
void TextureManager::ReloadAllLoadedTextures()
{
	for (auto& [filePath, textureData] : textureDatas) {
		TextureData reloadedTexture{};
		reloadedTexture.srvIndex = textureData.srvIndex;
		reloadedTexture.srvHandleCPU = textureData.srvHandleCPU;
		reloadedTexture.srvHandleGPU = textureData.srvHandleGPU;
		if (CreateTextureData(filePath, reloadedTexture, false)) {
			textureData = std::move(reloadedTexture);
		}
	}
}

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
			return false;
		}
	}

	textureData.filePath = filePath; //ファイルパス
	textureData.metadata = mipImages.GetMetadata(); //テクスチャメタデータを取得
	textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);

	// 初回だけSRVを確保し、再読込は同じSRV番号を書き換えます。
	if (allocateSrv) {
		textureData.srvIndex = srvManager_->Allocate();
		if (textureData.srvIndex == SrvManager::kInvalidSrvIndex) {
			return false;
		}
		textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
		textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);
	}

	//SRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = textureData.metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (textureData.metadata.IsCubemap()) {
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = UINT_MAX;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	} else
	{//2DTextureの設定
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = UINT(textureData.metadata.mipLevels);
	}

	//設定をもとにSRVの生成
	dxCommon_->GetDevice()->CreateShaderResourceView(textureData.resource.Get(), &srvDesc, textureData.srvHandleCPU);

	//テクスチャデータの転送(GPUへのキックから完了待ち、リセットまでをDirectXCommonに全て任せる)
	dxCommon_->ExecuteTextureTransfer(textureData.resource, mipImages);
	std::error_code error;
	textureData.lastWriteTime = std::filesystem::last_write_time(filePath, error);
	textureData.hasWriteTime = !error;
	return true;
}

const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
	//読み込み済みかチェック
	assert(textureDatas.contains(filePath));
	// 取得時に空の登録を作ると、後のLoadTextureが「読込済み」と誤認するためatで参照します。
	return textureDatas.at(filePath).metadata;
}

uint32_t TextureManager::GetSrvIndex(const std::string& filePath)
{
	// 読み込み済みかチェック
	assert(textureDatas.contains(filePath));
	return textureDatas.at(filePath).srvIndex;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath)
{
	// 読み込み済みかチェック
	assert(textureDatas.contains(filePath));
	return textureDatas.at(filePath).srvHandleGPU;
}
