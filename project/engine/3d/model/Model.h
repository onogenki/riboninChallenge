#pragma once
#include <array>
#include <cstdint>
#include <utility>
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

// モデルファイルから頂点・材質・骨とGPU資源を作り、Object3dへ描画データを貸します。
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

	// 骨・頂点のGPU資源が全部そろった時だけ、Object3dへSkinClusterを渡します。
	bool CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		const Skeleton& skeleton, const ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize,
		SkinCluster& result);

	// 読込済みNodeの親子関係から骨を作り、Object3dへ値として渡します。
	Skeleton CreateSkeleton(const Node& rootNode);

	// ファイルのKeyframeを読み、呼び出し元が所有するAnimationを返します。
	static Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

	// 借りたModelCommonで頂点・材質・画像を準備し、指定済み画像の欠損も含め失敗ならfalseを返します。
	bool Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);
	// 三つのGPU Bufferの存在だけを調べます。材質画像まで読めたかはInitializeの戻り値で判断します。
	bool IsLoaded() const { return vertexResource != nullptr && indexResource != nullptr && materialResource != nullptr; }

	// Animation適用後に、親子関係をたどって各骨の行列を更新します。
	void Update(Skeleton& skeleton);
	// 骨行列の更新後に、SkinClusterのGPU用Paletteへ書き込みます。
	void Update(SkinCluster& skinCluster,const Skeleton& skeleton);

	// Object3dが通常Pipelineを設定した後、骨なし頂点と材質を描きます。
	void Draw(uint32_t textureSrvIndexOverride = UINT32_MAX);
	// 鏡など専用Shader側で材質を設定する時に、頂点とIndexだけを描きます。
	void DrawGeometry();

	// Object3dが骨ありPipelineを設定した後、頂点の影響度とPaletteを渡して描きます。
	void Draw(const SkinCluster& skinCluster, uint32_t textureSrvIndexOverride = UINT32_MAX);
	// Compute方式を選んだ描画経路だけで、骨変形した頂点をGPU出力Bufferへ書き込みます。
	void DispatchSkinning(SkinCluster& skinCluster);
	// DispatchSkinning後の頂点Bufferを、専用Pipelineで描きます。
	void DrawSkinned(const SkinCluster& skinCluster, uint32_t textureSrvIndexOverride = UINT32_MAX);

	// 読込成功した画像を共有Modelの全材質へ適用します。個体専用ならObject3dのTexture上書きを使います。
	void SetTexture(const std::string& filePath);
	// モデルファイルを再読込する時、実行中に変えた材質と明示的な画像指定だけを新しいModelへ引き継ぎます。
	bool CopyRuntimeSettingsFrom(const Model& previous);
	// 共有Modelの材質で、色付きLightの反射だけを弱めます。個体ごとの設定ではありません。
	void SetSpecularIntensity(float intensity) {
		if (materialData) {
			materialData->specularIntensity = intensity;
		}
	}

	// 共有Modelの環境反射率を変更します。同じModelを使う別Objectにも値が及びます。
	void SetEnvironmentCoefficient(float coefficient) {
		if (materialData) {
			materialData->environmentCoefficient = coefficient;
		}
	}
	// 共有Modelの環境反射率を読み、材質Bufferがなければ0を返します。
	float GetEnvironmentCoefficient() const {
		if (materialData) {
			return materialData->environmentCoefficient;
		}
		return 0.0f;
	}

	// AnimationのNode名と骨を対応付け、Animationが生きている間だけ参照を借ります。
	void BuildAnimationMapping(Skeleton& skeleton, const Animation& animation);
	// 指定時刻のKeyframeを骨へ適用します。この後に骨行列とPaletteをUpdateします。
	void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);

	// 読込済みデータのconst参照を貸します。Model再読込後まで保持せず、外部から書き換えません。
	const ModelData& GetModelData() const { return modelData; }
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
	// SetTextureで明示された画像だけを記録し、モデルファイル側の画像変更とは区別します。
	std::optional<std::string> runtimeTextureOverride_;



	bool LoadModelFile(const std::string& directoryPath, const std::string& filename);
	bool CreateIndexData();
	bool CreateVertexData();
	bool CreateMaterialData();
	void ResetGpuResources();

};
