#pragma once
#include "SrvManager.h"
#include <string>
#include <unordered_map>
#include <filesystem>
#include <dxgi1_6.h>

#include "externals/DirectXTex/d3dx12.h"
#include "externals/DirectXTex/DirectXTex.h"

// 前方宣言
class DirectXCommon;
class SrvManager;

//テクスチャマネージャー
class TextureManager
{
public:
	//シングルトンインスタンスの取得
	static TextureManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	void Finalize();

	bool LoadTexture(const std::string& filePath);
	// 保存時刻が変わったTextureだけを、同じSRV番号のまま再読込します。
	// Frameworkから毎フレーム呼んでも、ファイルが変わらない限りGPU処理は行いません。
	void UpdateHotReload();
	// 保存時刻に関係なく、現在読み込み済みの全Textureを再読込します。
	void ReloadAllLoadedTextures();

	//メタデータを取得
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);
	//SRVインデックスの取得
	uint32_t GetSrvIndex(const std::string& filePath);
	//GPUハンドルの取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

	//テクスチャが読み込み済みかチェック
	bool Contains(const std::string& filePath) const {
		return textureDatas.contains(filePath);
	}

private:

	static TextureManager* instance;

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;

	//SRVインデックスの開始番号
	//static uint32_t kSRVIndexTop;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	//テクスチャ1枚分のデータ
	struct TextureData
	{
		std::string filePath;
		DirectX::TexMetadata metadata{};
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		uint32_t srvIndex = UINT32_MAX;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU{};
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU{};
		std::filesystem::file_time_type lastWriteTime{};
		bool hasWriteTime = false;
	};

	// 画像読込・GPU転送・SRV作成をまとめ、初回読込と再読込で共有します。
	bool CreateTextureData(const std::string& filePath, TextureData& textureData, bool allocateSrv);

	//テクスチャデータ
	std::unordered_map<std::string, TextureData>textureDatas;

};

