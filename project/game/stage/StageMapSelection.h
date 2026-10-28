#pragma once

// Stage IDと外部マップファイルの対応、および次のStageへの切替予約だけを管理します。
// JSON/CSVの読込はStageMapRepository、実行中のモデル所有はStageMapRuntimeへ任せます。
class StageMapSelection
{
public:
	struct Definition
	{
		int id = 1;
		const char* levelFileName = "stage1";
		const char* levelFilePath = "resources/levels/stage1.json";
		const char* mapChipFilePath = "resources/maps/stage1.csv";
	};

	// 初期Stage IDだけを記録し、最初の読込前にはファイル定義を選択しません。
	explicit StageMapSelection(int initialStageId = 1) : stageId_(initialStageId) {}

	// 未登録IDなら現在の選択を変えずfalseを返します。
	bool Select(int stageId);
	// 有効なIDだけを次のUpdateへ予約し、無効なIDならfalseを返します。
	bool Request(int stageId);
	// Updateの先頭で予約を一度だけ取り出し、予約がなければ0を返します。
	int TakeRequestedStageId();

	int GetStageId() const { return stageId_; }
	const Definition* GetDefinition() const { return definition_; }

private:
	// Stage番号とJSON/CSVの対応を一箇所に保ちます。
	static const Definition* FindDefinition(int stageId);

	int stageId_ = 1;
	int requestedStageId_ = 0;
	const Definition* definition_ = nullptr;
};
