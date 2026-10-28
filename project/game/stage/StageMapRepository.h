#pragma once

#include "LevelLoader.h"
#include "MapChipField.h"
#include "StageMapFileMonitor.h"
#include "StageMapSelection.h"
#include <functional>
#include <memory>
#include <string>

// 選択中StageのJSON・CSVと変更監視を所有し、読込失敗時は現在のデータを残します。
// 床・鏡・Cameraへの反映はSceneから受け取る関数に任せ、ゲーム内のObjectは所有しません。
class StageMapRepository
{
public:
	using ApplyLoadedData = std::function<bool()>;

	// 最初の読込前はStage番号だけを保持し、ファイル定義は選択しません。
	explicit StageMapRepository(int initialStageId = 1) : selection_(initialStageId) {}

	// @brief Stage番号を切り替えて読み込み、適用に失敗した場合は元の番号とデータを維持します。
	// @param stageId 選択するStage番号です。現在の定義では1〜5を受け付けます。
	// @param applyLoadedData 仮読込したJSON・CSVをSceneのObjectへ反映する関数です。
	// @param status 読込結果の説明を受け取ります。
	// @return 読込とSceneへの反映が両方成功した時だけtrueです。
	// @throws applyLoadedDataが例外を出した時は、選択番号と読込データを戻して再送出します。
	bool SelectAndReload(int stageId, const ApplyLoadedData& applyLoadedData, std::string& status);
	// @brief 現在選択中のJSON・CSVを仮読込し、Sceneへの反映成功後だけ採用します。
	// @param applyLoadedData 仮データを使って床・鏡・Camera等を反映する関数です。失敗時は旧データへ戻します。
	// @param status 読込結果の説明を受け取ります。
	// @return ファイル読込と反映が完了した時だけtrueです。
	// @throws applyLoadedDataが例外を出した時は、読込データを戻して再送出します。
	bool Reload(const ApplyLoadedData& applyLoadedData, std::string& status);
	// @brief 現在の編集データをJSONへ保存し、成功時だけ監視時刻を合わせます。
	// @param status 保存結果の説明を受け取ります。
	// @return データがあり、選択中StageのJSON保存に成功した時だけtrueです。
	bool Save(std::string& status);
	// 読込に成功したStageのJSON・CSVを、新しい監視対象にします。
	void StartWatchingCurrentStage();
	// 外部保存を一度だけ通知し、Sceneが安全なUpdate順で再読込できるようにします。
	bool ConsumeFileChange() { return fileMonitor_.ConsumeChange(); }
	// Editorの手動再読込後、同じ変更を次Frameで繰り返しません。
	void SynchronizeJson() { fileMonitor_.SynchronizeJson(); }
	// Stage変更要求は次のScene Updateへ渡し、実行中のObjectをここでは触りません。
	bool RequestStage(int stageId) { return selection_.Request(stageId); }
	int TakeRequestedStageId() { return selection_.TakeRequestedStageId(); }
	int GetStageId() const { return selection_.GetStageId(); }
	const std::string& GetJsonFilePath() const { return fileMonitor_.GetJsonFilePath(); }
	LevelLoader::LevelData* GetLevelData() { return levelData_.get(); }
	const LevelLoader::LevelData* GetLevelData() const { return levelData_.get(); }
	const MapChipField& GetMapChipField() const { return mapChipField_; }
	// Scene終了時に読込データだけを解放し、GPU側のObjectは各所有者が解放します。
	void Clear();

private:
	StageMapSelection selection_{};
	StageMapFileMonitor fileMonitor_{};
	std::unique_ptr<LevelLoader::LevelData> levelData_;
	MapChipField mapChipField_{};
};
