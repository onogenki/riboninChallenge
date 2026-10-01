#pragma once

#include "LevelLoader.h"
#include <memory>
#include <string>
#include <vector>

class CarryableMirror;
class FixedMirror;
class Object3dCommon;

// 携帯鏡・鏡床・Level配置の固定鏡を生成する、ゲーム側のFactoryです。
// JSONの読込はLevelLoader、見た目とColliderは各鏡、生成後の所有と更新順はSceneが担当します。
class StageMirrorFactory
{
public:
	// 未所持の携帯鏡を生成します。失敗時はnullptr、成功時は呼出元へ所有権を返します。
	static std::unique_ptr<CarryableMirror> CreateCarryableMirror(
		Object3dCommon* object3dCommon,
		const std::string& modelName,
		const Vector3& position,
		float width,
		float height);
	// 水平で両面のLaserを反射する鏡床を生成します。反射Textureは従来通り256です。
	// 失敗時はnullptr。鏡床なしで続行するかどうかは呼出元のSceneが判断します。
	static std::unique_ptr<FixedMirror> CreateMirrorFloor(
		Object3dCommon* object3dCommon,
		const std::string& modelName,
		const Vector3& position,
		float width,
		float height);
	// データ順に鏡を生成します。出力はSceneが用意する仮一覧で、成功後に本番一覧へ移します。
	// 失敗時はfalseを返します。出力に生成済みの鏡が残るため、本番一覧には渡さないでください。
	static bool CreateFixedMirrors(
		Object3dCommon* object3dCommon,
		const std::vector<const LevelLoader::ObjectData*>& mirrorDataList,
		std::vector<std::unique_ptr<FixedMirror>>& outFixedMirrors);
	// 同じ枚数の既存Mirrorへ位置・Y回転・サイズ・Colliderを反映します。GPU資源は作り直しません。
	// Sceneは枚数の変更やモデル再読込が必要な場合、こちらではなくCreateFixedMirrorsを使います。
	static bool ApplyFixedMirrorEdits(
		const std::vector<const LevelLoader::ObjectData*>& mirrorDataList,
		std::vector<std::unique_ptr<FixedMirror>>& fixedMirrors);
};
