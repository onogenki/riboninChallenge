#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <cassert>

using namespace MyMath;

// Skeletonの構築、Animationファイル読込、Keyframe補間と適用をまとめる。
// モデルの頂点読込・GPU描画とは分け、Animationだけを追えるようにする。
namespace {
	Vector3 InterpolateKeyframeValue(const Vector3& start, const Vector3& end, float t)
	{
		return Lerp(start, end, t);
	}

	Quaternion InterpolateKeyframeValue(const Quaternion& start, const Quaternion& end, float t)
	{
		return Slerp(start, end, t);
	}

	template<typename tValue>
	tValue CalculateKeyframeValue(const std::vector<Model::Keyframe<tValue>>& keyframes, float time)
	{
		assert(!keyframes.empty());
		if (keyframes.size() == 1 || time <= keyframes.front().time) {
			return keyframes.front().value;
		}

		for (size_t index = 0; index < keyframes.size() - 1; ++index) {
			const size_t nextIndex = index + 1;
			if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
				const float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
				return InterpolateKeyframeValue(keyframes[index].value, keyframes[nextIndex].value, t);
			}
		}

		return keyframes.back().value;
	}
}

Model::Skeleton Model::CreateSkeleton(const Node& rootNode)
{
	Skeleton skeleton;
	skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

	//名前とindexのマッピングを行いアクセスしやすくなる
	for (const Joint& joint : skeleton.joints)
	{
		skeleton.jointMap.emplace(joint.name, joint.index);
	}

	//作成直後にも一度更新をかけておくと初期姿勢が正しく計算される
	Update(skeleton);

	return skeleton;
}

int32_t Model::CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints)
{
	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix;
	joint.skeletonSpaceMatrix = MakeIdentity4x4();
	joint.transform = node.transform;
	joint.index = uint32_t(joints.size());//現在登録されてる数をindexに
	joint.parent = parent;

	joints.push_back(joint);//skeletonのJoint列に追加

	for (const Node& child : node.children)
	{
		//子Jointを作成し、そのIndexを登録
		int32_t childIndex = CreateJoint(child, joint.index, joints);
		joints[joint.index].children.push_back(childIndex);//push_backなどで再確保が走るとアドレス変わるためindex
	}
	//自身のIndexを返す
	return joint.index;
}

Model::Animation Model::LoadAnimationFile(const std::string& directoryPath, const std::string& filename)
{
	//アニメーション
	Animation animation;
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);

	if (!scene || scene->mNumAnimations == 0) {
		return animation;
	}
	aiAnimation* animationAssimp = scene->mAnimations[0];  //周波数における長さ/周波数

	const double ticksPerSecond = animationAssimp->mTicksPerSecond != 0.0 ? animationAssimp->mTicksPerSecond : 1.0;
	animation.duration = float(animationAssimp->mDuration / ticksPerSecond);//時間の単位を秒に変換

	//NodeAnimation解析する
	for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex)
	{
		aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];

		NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex)
		{
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(keyAssimp.mTime / ticksPerSecond);//秒に変換
			keyframe.value = { -keyAssimp.mValue.x,keyAssimp.mValue.y,keyAssimp.mValue.z };//右手->左手
			nodeAnimation.translate.keyframes.push_back(keyframe);
		}
		//RotateはmNumRotationKeys/mRotationKeys,ScaleはmNumScalingKeys/mScalingKeysで取得できるので同様に行う

		//Rotate
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex)
		{
			aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
			KeyframeQuaternion keyframe;
			keyframe.time = float(keyAssimp.mTime / ticksPerSecond); // 秒に変換
			// 右手->左手 (Quaternionの場合は y と z を反転)
			//RotateはQuaternionで右手->左手に変換するために、yとzを反転させる必要がある。
			keyframe.value = { keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w };

			nodeAnimation.rotate.keyframes.push_back(keyframe);
		}

		//Scale
		for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex)
		{
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = float(keyAssimp.mTime / ticksPerSecond); // 秒に変換
			// Scale は変換不要　scaleはそのままでよい。
			keyframe.value = { keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z };

			nodeAnimation.scale.keyframes.push_back(keyframe);
		}
	}
	//解析完了
	return animation;
}

Vector3 Model::CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time)
{
	return CalculateKeyframeValue(keyframes, time);
}

Quaternion Model::CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time)
{
	return CalculateKeyframeValue(keyframes, time);
}

void Model::Update(Skeleton& skeleton)
{
	//すべてのJointを更新 親が若いので通常ループで処理可能になっている
	for (Joint& joint : skeleton.joints)
	{
		joint.localMatrix = MakeAffineMatrixQuaternion(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
		if (joint.parent)//親がいれば親の行列をかける
		{
			joint.skeletonSpaceMatrix = Multiply(joint.localMatrix, skeleton.joints[*joint.parent].skeletonSpaceMatrix);
		} else {//親がいないのでlocalMatrixとskeletonSpaceMatrixは一致する
			joint.skeletonSpaceMatrix = joint.localMatrix;
		}
	}
}

void Model::BuildAnimationMapping(Skeleton& skeleton, const Animation& animation)
{
	skeleton.animationNodeMap.clear();
	skeleton.animationNodeMap.resize(skeleton.joints.size(), nullptr);

	for (const Joint& joint : skeleton.joints) {
		if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end()) {
			skeleton.animationNodeMap[joint.index] = &it->second;
		}
	}

	skeleton.cachedAnimation = &animation;
}

void Model::ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime)
{
	if (skeleton.cachedAnimation != &animation || skeleton.animationNodeMap.size() != skeleton.joints.size()) {
		BuildAnimationMapping(skeleton, animation);
	}

	for (Joint& joint : skeleton.joints)
	{
		//対象のJointのAnimationがあれば値の適用を行う。初期化付きif文
		const NodeAnimation* rootNodeAnimation = skeleton.animationNodeMap[joint.index];
		if (rootNodeAnimation)
		{
			if (!rootNodeAnimation->translate.keyframes.empty()) {
				joint.transform.translate = CalculateValue(rootNodeAnimation->translate.keyframes, animationTime);
			}
			if (!rootNodeAnimation->rotate.keyframes.empty()) {
				joint.transform.rotate = CalculateValue(rootNodeAnimation->rotate.keyframes, animationTime);
			}
			if (!rootNodeAnimation->scale.keyframes.empty()) {
				joint.transform.scale = CalculateValue(rootNodeAnimation->scale.keyframes, animationTime);
			}
		}
	}
}
