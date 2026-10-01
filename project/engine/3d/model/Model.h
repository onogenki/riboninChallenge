#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <wrl.h>
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "ModelCommon.h"
#include "MyMath.h"
#include <map>
#include<optional>
#include <span>

struct aiNode;

//見た目のモデル
class Model
{
public:
	static const uint32_t kNumMaxInfluence = 4;

	//キーフレーム
	template<typename tValue>
	struct Keyframe
	{
		float time = 0.0f;//キーフレームの時刻
		tValue value{};//キーフレームの幅
	};
	using KeyframeVector3 = Keyframe<Vector3>;
	using KeyframeQuaternion = Keyframe<Quaternion>;

	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
		float shininess;
		float environmentCoefficient;
		float specularIntensity;
		float padding2;
	};

	struct DirectionalLight {
		Vector4 color;
		Vector3 direction;
		float intensity;
	};

	struct MaterialData {
		std::string textureFilePath;
	};

	struct MeshData {
		uint32_t indexOffset = 0;
		uint32_t indexCount = 0;
		uint32_t materialIndex = 0;
	};

	struct Node
	{
		QuaternionTransform transform{};
		Matrix4x4 localMatrix{};
		std::string name;
		std::vector<Node> children;
	};

	struct VertexWeightData
	{
		float weight = 0.0f;
		uint32_t vertexIndex = 0;
	};

	struct JointWeightData
	{
		Matrix4x4 inverseBindPoseMatrix{};
		std::vector<VertexWeightData> vertexWeights;
	};

	struct VertexInfluence
	{
		std::array<float, kNumMaxInfluence> weights{};
		std::array<int32_t, kNumMaxInfluence>jointIndices{};
	};

	struct WellForGPU
	{
		Matrix4x4 skeletonSpaceMatrix;//位置用
		Matrix4x4 skeletonSpaceInverseTransposeMatrix;//法線用
	};

	struct SkinCluster
	{
		struct SkinningInformation
		{
			uint32_t numVertices = 0;
		};

		std::vector<Matrix4x4>inverseBindPoseMatrices;
		Microsoft::WRL::ComPtr<ID3D12Resource>influenceResource;
		D3D12_VERTEX_BUFFER_VIEW influenceBufferView{};
		std::span<VertexInfluence>mappedInfluence;
		Microsoft::WRL::ComPtr<ID3D12Resource>paletteResource;
		std::span<WellForGPU>mappedPalette;
		uint32_t paletteSrvIndex = UINT32_MAX;
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>paletteSrvHandle{};
		uint32_t inputVertexSrvIndex = UINT32_MAX;
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>inputVertexSrvHandle{};
		uint32_t influenceSrvIndex = UINT32_MAX;
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>influenceSrvHandle{};
		Microsoft::WRL::ComPtr<ID3D12Resource>outputVertexResource;
		D3D12_VERTEX_BUFFER_VIEW outputVertexBufferView{};
		uint32_t outputVertexUavIndex = UINT32_MAX;
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>outputVertexUavHandle{};
		D3D12_RESOURCE_STATES outputVertexResourceState = D3D12_RESOURCE_STATE_COMMON;
		Microsoft::WRL::ComPtr<ID3D12Resource>skinningInformationResource;
		SkinningInformation* mappedSkinningInformation = nullptr;
	};

	struct ModelData {
		std::map<std::string, JointWeightData> skinClusterData;
		std::vector<VertexData> vertices;
		std::vector<uint32_t>indices;
		std::vector<MaterialData> materials;
		std::vector<MeshData> meshes;
		Node rootNode;
	};

	template<typename tValue>
	struct AnimationCurve
	{
		std::vector<Keyframe<tValue>> keyframes;
	};

	struct NodeAnimation
	{
		AnimationCurve<Vector3> translate;
		AnimationCurve<Quaternion>rotate;
		AnimationCurve<Vector3> scale;
	};

	struct Animation
	{
		float duration = 0.0f;//アニメーション全体の尺(単位は秒)
		//NodeAnimationの集合。Node名でひけるようにしておく
		std::map<std::string, NodeAnimation>nodeAnimations;
	};

	struct Joint
	{
		QuaternionTransform transform{};//Transform情報
		Matrix4x4 localMatrix{};
		Matrix4x4 skeletonSpaceMatrix{};//skeletonSpaceでの変換行列
		std::string name;
		std::vector<int32_t> children;//子JointのIndexのリスト。いなければ空
		int32_t index = -1;
		std::optional<int32_t>parent;//親JointのIndex、いなければnull
	};

	struct Skeleton
	{
		std::vector<const NodeAnimation*> animationNodeMap;
		const Animation* cachedAnimation = nullptr;
		int32_t root = -1;//RootJointのIndex
		std::map<std::string, int32_t>jointMap;//Joint名とIndexとの辞書
		std::vector<Joint>joints;//所属しているジョイント
	};

	SkinCluster CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		const Skeleton& skeleton, const ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize);

	Skeleton CreateSkeleton(const Node& rootNode);

	//アニメーションの解析
	static Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

	bool Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);
	bool IsLoaded() const { return vertexResource != nullptr && indexResource != nullptr && materialResource != nullptr; }

	//skeletonの更新
	void Update(Skeleton& skeleton);
	void Update(SkinCluster& skinCluster,const Skeleton& skeleton);

	// 通常モデル描画用（骨なし）
	void Draw(uint32_t textureSrvIndexOverride = UINT32_MAX);
	// 専用Shader側で材質を設定する描画用に、頂点とIndexだけを描画する
	void DrawGeometry();

	// スキニングモデル描画用（骨あり）
	void Draw(const SkinCluster& skinCluster, uint32_t textureSrvIndexOverride = UINT32_MAX);
	void DispatchSkinning(SkinCluster& skinCluster);
	void DrawSkinned(const SkinCluster& skinCluster, uint32_t textureSrvIndexOverride = UINT32_MAX);

	void SetTexture(const std::string& filePath);
	// 色付きLightの反射だけを弱め、Texture本来の色を見えやすくします。
	void SetSpecularIntensity(float intensity) {
		if (materialData) {
			materialData->specularIntensity = intensity;
		}
	}

	void SetEnvironmentCoefficient(float coefficient) {
		if (materialData) {
			materialData->environmentCoefficient = coefficient;
		}
	}
	float GetEnvironmentCoefficient() const {
		if (materialData) {
			return materialData->environmentCoefficient;
		}
		return 0.0f;
	}

	//アニメーション適用
	void BuildAnimationMapping(Skeleton& skeleton, const Animation& animation);
	void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);

	ModelData& GetModelData() { return modelData; }
private:
	// ファイル読込とAnimation内部でだけ使う補助関数。
	int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);
	static Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
	static Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);
	Node ReadNode(aiNode* node);

	//ModelCommonのポインタ
	ModelCommon* modelCommon_ = nullptr;
	//OBJファイルのデータ
	ModelData modelData;

	//インデックスリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	VertexData* vertexData = nullptr;
	//マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr;



	bool LoadModelFile(const std::string& directoryPath, const std::string& filename);
	void CreateIndexData();
	void CreateVertexData();
	void CreateMaterialData();

};
