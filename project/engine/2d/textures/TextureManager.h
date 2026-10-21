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

// TextureとSRV番号を一元管理し、SpriteやModelへ読込済みTextureを渡します。
class TextureManager
{
public:
	//シングルトンインスタンスの取得
	static TextureManager* GetInstance();

	// @brief 初期化済みのDirectXとSRV管理を借り、Texture登録を始めます。
	// @param dxCommon GPU資源を生成する共通部。TextureManagerより長く生存させます。
	// @param srvManager SRV番号の所有者。Finalizeまで生存させます。
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	// 所有する全TextureのSRVを返却し、登録を消します。
	void Finalize();

	// @brief 読込とSRV作成に成功したTextureだけを登録し、失敗を空の登録で隠しません。
	// @param filePath 実行時の作業ディレクトリから参照する画像パスです。
	// @return 読込済み、または今回の読込・登録に成功した時だけtrueです。
	bool LoadTexture(const std::string& filePath);
	// 保存時刻が変わったTextureだけを、同じSRV番号のまま再読込します。
	// Frameworkから毎フレーム呼んでも、ファイルが変わらない限りGPU処理は行いません。
	void UpdateHotReload();
	// 保存時刻に関係なく、現在読み込み済みの全Textureを再読込します。
	void ReloadAllLoadedTextures();

	// @brief 登録済みTextureの寸法・形式を借りて返します。取得だけでは登録しません。
	// @param filePath LoadTextureがtrueを返した画像パスです。
	// @return 登録済みTextureのメタデータへの参照です。未登録名はDebugでassertし、他構成ではatが例外を投げます。
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);
	// @brief 登録済みTextureのSRV番号を返します。取得だけでは登録しません。
	// @param filePath LoadTextureがtrueを返した画像パスです。
	// @return 登録済みTextureのSRV番号です。未登録名はDebugでassertし、他構成ではatが例外を投げます。
	uint32_t GetSrvIndex(const std::string& filePath);
	// @brief 登録済みTextureのGPUハンドルを返します。取得だけでは登録しません。
	// @param filePath LoadTextureがtrueを返した画像パスです。
	// @return 登録済みTextureのGPUハンドルです。未登録名はDebugでassertし、他構成ではatが例外を投げます。
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

	// 未登録画像を追加せず、読込済みかだけを確認します。
	bool Contains(const std::string& filePath) const {
		return textureDatas_.contains(filePath);
	}

private:
	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	//テクスチャ1枚分のデータ
	struct TextureData
	{
		std::string filePath_;
		DirectX::TexMetadata metadata_{};
		Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
		uint32_t srvIndex_ = UINT32_MAX;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_{};
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};
		std::filesystem::file_time_type lastWriteTime_{};
		bool hasWriteTime_ = false;
	};

	// 画像読込・GPU転送・SRV作成をまとめ、初回読込と再読込で共有します。
	bool CreateTextureData(const std::string& filePath, TextureData& textureData, bool allocateSrv);
	// 既存のSRV番号を保って再読込し、成功時だけ登録内容を差し替えます。
	void ReloadTexture(const std::string& filePath, TextureData& textureData);

	//テクスチャデータ
	std::unordered_map<std::string, TextureData> textureDatas_;

};

