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

namespace Engine
{

// モデルファイルから頂点・材質・骨とGPU資源を作り、Object3dへ描画データを貸します。
class Model
{
public:
	static const uint32_t kNumMaxInfluence = 4;

	// 一つの時刻と値を持ち、AnimationCurveの補間元として使います。
	template<typename tValue>
	struct Keyframe
	{
		float time = 0.0f;//キーフレームの時刻
		tValue value{};//キーフレームの幅
	};
	using KeyframeVector3 = Keyframe<Vector3>;
	using KeyframeQuaternion = Keyframe<Quaternion>;

	// モデルファイルから読んだ頂点を、通常描画と骨変形の共通入力として保持します。
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// HLSLへ渡す材質値です。GPUのConstant Bufferと並びを合わせます。
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

	// Meshが参照する材質画像のパスを、モデル読込後も保持します。
	struct MaterialData {
		std::string textureFilePath;
	};

	// 一つの材質で描くIndex範囲を表し、Draw時のTexture選択にも使います。
	struct MeshData {
		uint32_t indexOffset = 0;
		uint32_t indexCount = 0;
		uint32_t materialIndex = 0;
	};

	// 読込元モデルの親子階層を保持し、Skeleton生成の入力にします。
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

	// 骨付きObject3dが一体ごとに所有する、Weight・Palette・GPU Descriptorの一式です。
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

	// 読込済みの頂点・材質・骨情報をまとめ、共有Modelが所有します。
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

	// Node名ごとの動きと秒単位の長さを持ち、Object3dが再生中の値を所有します。
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

	// Object3d一体の骨姿勢を保持し、共有ModelのNode階層から作ります。
	struct Skeleton
	{
		std::vector<const NodeAnimation*> animationNodeMap;
		const Animation* cachedAnimation = nullptr;
		int32_t root = -1;//RootJointのIndex
		std::map<std::string, int32_t>jointMap;//Joint名とIndexとの辞書
		std::vector<Joint>joints;//所属しているジョイント
	};

	// @brief 骨・頂点のGPU資源が全部そろった時だけ、Object3dへSkinClusterを渡します。
	// @param device 描画に使うDeviceです。
	// @param skeleton 対象Objectの骨です。
	// @param modelData 共有Modelから読む頂点・Weightです。
	// @param descriptorHeap 描画で参照するDescriptor Heapです。
	// @param descriptorSize Descriptor一枠の大きさです。
	// @param result 成功時だけ完成した資源へ置き換え、Object3dが所有します。
	// @return 必要な資源・Descriptorを準備できた時だけtrueです。失敗時は途中資源を片付けます。
	bool CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device,
		const Skeleton& skeleton, const ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize,
		SkinCluster& result);

	// @brief 読込済みNodeの親子関係から骨を作り、Object3dへ値として渡します。
	// @param rootNode 共有Modelが保持する階層の根です。
	// @return 親子Indexと初期姿勢を計算済みのSkeletonです。
	Skeleton CreateSkeleton(const Node& rootNode);

	// @brief ファイルの最初のAnimationを秒単位のKeyframeへ変換します。
	// @param directoryPath 読込元のフォルダです。
	// @param filename 読込むファイル名です。
	// @return 呼び出し元が所有するAnimationです。読込不能・Animationなしならdurationは0です。
	static Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

	// @brief 借りた描画共通設定で、モデル・GPU Buffer・材質画像を順に準備します。
	// @param modelCommon Modelより長く生きる描画共通設定で、所有権は移しません。
	// @param directoryPath モデルファイルのフォルダです。
	// @param filename モデルファイル名です。
	// @return すべて準備できた時だけtrueです。指定画像の欠損も失敗として返します。
	bool Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);
	// 三つのGPU Bufferの存在だけを調べます。材質画像まで読めたかはInitializeの戻り値で判断します。
	bool IsLoaded() const { return vertexResource_ != nullptr && indexResource_ != nullptr && materialResource_ != nullptr; }

	// @brief Animation適用後に、親子関係をたどって各骨の行列を更新します。
	// @param skeleton 対象Objectが所有する骨です。計算結果を同じ値へ書き込みます。
	void Update(Skeleton& skeleton);
	// @brief 骨行列の更新後に、SkinClusterのGPU用Paletteへ書き込みます。
	// @param skinCluster 対象Objectが所有するGPU資源です。
	// @param skeleton Update(Skeleton&)で更新済みの骨です。
	void Update(SkinCluster& skinCluster,const Skeleton& skeleton);

	// @brief Object3dが通常Pipelineを設定した後、骨なし頂点と材質を描きます。
	// @param textureSrvIndexOverride UINT32_MAXなら材質ごとの画像、そうでなければ個体指定のSRVです。
	void Draw(uint32_t textureSrvIndexOverride = UINT32_MAX);
	// 鏡など専用Shader側で材質を設定する時に、頂点とIndexだけを描きます。
	void DrawGeometry();

	// @brief Object3dが骨ありPipelineを設定した後、頂点の影響度とPaletteを渡して描きます。
	// @param skinCluster 対象Objectが所有する骨変形用資源です。
	// @param textureSrvIndexOverride UINT32_MAXなら材質ごとの画像、そうでなければ個体指定のSRVです。
	void Draw(const SkinCluster& skinCluster, uint32_t textureSrvIndexOverride = UINT32_MAX);
	// @brief Compute方式を選んだ描画経路だけで、骨変形した頂点をGPU出力Bufferへ書き込みます。
	// @param skinCluster UpdateでPaletteを反映済みの、対象Object専用資源です。
	void DispatchSkinning(SkinCluster& skinCluster);
	// @brief DispatchSkinning後の頂点Bufferを、専用Pipelineで描きます。
	// @param skinCluster Compute結果を保持する、対象Object専用資源です。
	// @param textureSrvIndexOverride UINT32_MAXなら材質ごとの画像、そうでなければ個体指定のSRVです。
	void DrawSkinned(const SkinCluster& skinCluster, uint32_t textureSrvIndexOverride = UINT32_MAX);

	// @brief 読込成功した画像を共有Modelの全材質へ適用します。個体専用ならObject3dを使います。
	// @param filePath 材質へ設定する画像です。読込失敗時は従来の設定を保ちます。
	void SetTexture(const std::string& filePath);
	// @brief ファイル由来の形状を保ち、実行中に変えた材質と明示画像だけを引き継ぎます。
	// @param previous 再読込前のModelです。関数が戻るまで生きている必要があります。
	// @return 両Modelの材質があり、明示画像も利用可能な時だけtrueです。
	bool CopyRuntimeSettingsFrom(const Model& previous);
	// @brief 共有Modelの材質で、色付きLightの反射だけを弱めます。個体設定ではありません。
	// @param intensity 材質Shaderへ渡す反射強度です。
	void SetSpecularIntensity(float intensity) {
		if (materialData_) {
			materialData_->specularIntensity = intensity;
		}
	}

	// @brief 共有Modelの環境反射率を変更します。同じModelを使う別Objectにも及びます。
	// @param coefficient 材質Shaderへ渡す環境反射率です。
	void SetEnvironmentCoefficient(float coefficient) {
		if (materialData_) {
			materialData_->environmentCoefficient = coefficient;
		}
	}
	// 共有Modelの環境反射率を読み、材質Bufferがなければ0を返します。
	float GetEnvironmentCoefficient() const {
		if (materialData_) {
			return materialData_->environmentCoefficient;
		}
		return 0.0f;
	}

	// @brief AnimationのNode名と骨を対応付け、毎フレームの名前検索を避けます。
	// @param skeleton 対象Objectが所有する骨です。対応表を書き換えます。
	// @param animation 再生中の動きです。対応表の使用中は同じ値を生かします。
	void BuildAnimationMapping(Skeleton& skeleton, const Animation& animation);
	// @brief 指定時刻のKeyframeを骨へ適用し、次のUpdateで行列とPaletteへ反映します。
	// @param skeleton 対象Objectの骨です。
	// @param animation 再生中の動きです。
	// @param animationTime 補間に使う秒単位の時刻です。
	void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);

	// 読込済みデータのconst参照を貸します。Model再読込後まで保持せず、外部から書き換えません。
	const ModelData& GetModelData() const { return modelData_; }
private:
	// ファイル読込とAnimation内部でだけ使う補助関数。
	int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);
	static Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
	static Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);
	Node ReadNode(aiNode* node);

	//ModelCommonのポインタ
	ModelCommon* modelCommon_ = nullptr;
	//OBJファイルのデータ
	ModelData modelData_;

	//インデックスリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	//バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	VertexData* vertexData_ = nullptr;
	//マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	// SetTextureで明示された画像だけを記録し、モデルファイル側の画像変更とは区別します。
	std::optional<std::string> runtimeTextureOverride_;



	bool LoadModelFile(const std::string& directoryPath, const std::string& filename);
	bool CreateIndexData();
	bool CreateVertexData();
	bool CreateMaterialData();
	void ResetGpuResources();

};

}
