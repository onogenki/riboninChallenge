#include "SceneEditor.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>

namespace {

	// 拡張子から、棚でモデルとして扱うファイルかを判定する。
	bool IsShelfModelFile(const std::filesystem::path& path)
	{
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
			return static_cast<char>(std::tolower(character));
		});
		return extension == ".obj" || extension == ".gltf" || extension == ".glb" || extension == ".fbx";
	}

	// 拡張子から、棚でテクスチャとして扱うファイルかを判定する。
	bool IsShelfTextureFile(const std::filesystem::path& path)
	{
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
			return static_cast<char>(std::tolower(character));
		});
		return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp";
	}

	// Assimp専用の座標を、エンジン共通のVector3へ変換する。
	Vector3 ToShelfVector3(const aiVector3D& value)
	{
		return { value.x, value.y, value.z };
	}
}

// resources配下を走査し、モデル棚に表示する情報とサムネイル形状を作る。
void SceneEditor::ScanResourceShelf(ShelfState& state)
{
	state.entries.clear();

	const std::filesystem::path resourceDirectory = "resources";
	std::error_code errorCode;
	if (!std::filesystem::exists(resourceDirectory, errorCode)) {
		state.selectedEntry.clear();
		return;
	}

	for (std::filesystem::recursive_directory_iterator it(resourceDirectory, std::filesystem::directory_options::skip_permission_denied, errorCode), end;
		it != end;
		it.increment(errorCode)) {
		if (errorCode) {
			errorCode.clear();
			continue;
		}
		// 撮影結果は配置用素材ではないため、Title初期化で全画像をTexture登録しません。
		// フォルダ自体は残し、CaptureManagerからの保存・再生には影響させません。
		if (it->is_directory(errorCode) && it->path().filename() == "Captures") {
			it.disable_recursion_pending();
			continue;
		}
		if (errorCode) {
			errorCode.clear();
			continue;
		}
		if (!it->is_regular_file(errorCode) || errorCode) {
			errorCode.clear();
			continue;
		}

		const bool isModelFile = IsShelfModelFile(it->path());
		const bool isTextureFile = IsShelfTextureFile(it->path());
		if (!isModelFile && !isTextureFile) {
			continue;
		}

		std::filesystem::path relativePath = std::filesystem::relative(it->path(), resourceDirectory, errorCode);
		if (errorCode) {
			relativePath = it->path().filename();
			errorCode.clear();
		}
		std::filesystem::path displayPath = relativePath;
		displayPath.replace_extension();

		ShelfEntry shelfEntry{};
		shelfEntry.fileName = relativePath.generic_string();
		shelfEntry.displayName = displayPath.generic_string();

		if (isTextureFile) {
			shelfEntry.isTexture = true;
			shelfEntry.textureFilePath = (std::filesystem::path("Resources") / relativePath).generic_string();
			TextureManager* textureManager = TextureManager::GetInstance();
			if (!textureManager->Contains(shelfEntry.textureFilePath)) {
				textureManager->LoadTexture(shelfEntry.textureFilePath);
			}
			shelfEntry.textureSrvIndex = textureManager->GetSrvIndex(shelfEntry.textureFilePath);
			const DirectX::TexMetadata& metadata = textureManager->GetMetaData(shelfEntry.textureFilePath);
			shelfEntry.textureSize = {
				static_cast<float>(metadata.width),
				static_cast<float>(metadata.height)
			};
			state.entries.push_back(std::move(shelfEntry));
			continue;
		}

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(it->path().string(), aiProcess_Triangulate | aiProcess_GenNormals);
		if (scene) {
			shelfEntry.hasMesh = scene->HasMeshes();
			shelfEntry.hasAnimation = scene->mNumAnimations > 0;

			Vector3 minPoint{
				(std::numeric_limits<float>::max)(),
				(std::numeric_limits<float>::max)(),
				(std::numeric_limits<float>::max)()
			};
			Vector3 maxPoint{
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest()
			};
			bool hasVertex = false;
			bool hasDrawableFace = false;
			for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
				const aiMesh* mesh = scene->mMeshes[meshIndex];
				if (!mesh || !mesh->HasPositions()) {
					continue;
				}
				hasDrawableFace = hasDrawableFace || mesh->HasFaces();
				for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
					const Vector3 position = ToShelfVector3(mesh->mVertices[vertexIndex]);
					minPoint.x = (std::min)(minPoint.x, position.x);
					minPoint.y = (std::min)(minPoint.y, position.y);
					minPoint.z = (std::min)(minPoint.z, position.z);
					maxPoint.x = (std::max)(maxPoint.x, position.x);
					maxPoint.y = (std::max)(maxPoint.y, position.y);
					maxPoint.z = (std::max)(maxPoint.z, position.z);
					hasVertex = true;
				}
			}

			if (hasVertex) {
				shelfEntry.thumbnailCenter = {
					(minPoint.x + maxPoint.x) * 0.5f,
					(minPoint.y + maxPoint.y) * 0.5f,
					(minPoint.z + maxPoint.z) * 0.5f
				};
				const Vector3 size{
					maxPoint.x - minPoint.x,
					maxPoint.y - minPoint.y,
					maxPoint.z - minPoint.z
				};
				shelfEntry.thumbnailRadius = (std::max)({ size.x, size.y, size.z, 0.001f }) * 0.5f;
			}
			shelfEntry.canLoad = shelfEntry.hasMesh && hasVertex && hasDrawableFace;

			constexpr size_t maxThumbnailLines = 96;
			constexpr size_t maxThumbnailTriangles = 72;
			for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes && shelfEntry.thumbnailLines.size() < maxThumbnailLines; ++meshIndex) {
				const aiMesh* mesh = scene->mMeshes[meshIndex];
				if (!mesh || !mesh->HasPositions() || !mesh->HasFaces()) {
					continue;
				}
				for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
					const aiFace& face = mesh->mFaces[faceIndex];
					if (face.mNumIndices < 2) {
						continue;
					}
					if (face.mNumIndices >= 3 && shelfEntry.thumbnailTriangles.size() < maxThumbnailTriangles) {
						for (uint32_t index = 1; index + 1 < face.mNumIndices && shelfEntry.thumbnailTriangles.size() < maxThumbnailTriangles; ++index) {
							const uint32_t index0 = face.mIndices[0];
							const uint32_t index1 = face.mIndices[index];
							const uint32_t index2 = face.mIndices[index + 1];
							if (index0 < mesh->mNumVertices && index1 < mesh->mNumVertices && index2 < mesh->mNumVertices) {
								shelfEntry.thumbnailTriangles.push_back({
									ToShelfVector3(mesh->mVertices[index0]),
									ToShelfVector3(mesh->mVertices[index1]),
									ToShelfVector3(mesh->mVertices[index2])
								});
							}
						}
					}
					for (uint32_t index = 0; index < face.mNumIndices && shelfEntry.thumbnailLines.size() < maxThumbnailLines; ++index) {
						const uint32_t startIndex = face.mIndices[index];
						const uint32_t endIndex = face.mIndices[(index + 1) % face.mNumIndices];
						if (startIndex >= mesh->mNumVertices || endIndex >= mesh->mNumVertices) {
							continue;
						}
						shelfEntry.thumbnailLines.emplace_back(
							ToShelfVector3(mesh->mVertices[startIndex]),
							ToShelfVector3(mesh->mVertices[endIndex]));
					}
				}
			}
		}

		state.entries.push_back(std::move(shelfEntry));
	}

	std::sort(state.entries.begin(), state.entries.end(), [](const ShelfEntry& lhs, const ShelfEntry& rhs) {
		return lhs.fileName < rhs.fileName;
	});
	if (!state.selectedEntry.empty()) {
		const bool selectionStillExists = std::any_of(state.entries.begin(), state.entries.end(), [&](const ShelfEntry& entry) {
			return entry.fileName == state.selectedEntry;
		});
		if (!selectionStillExists) {
			state.selectedEntry.clear();
		}
	}
}
