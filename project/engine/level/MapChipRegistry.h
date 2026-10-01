#pragma once

#include "MapChipField.h"
#include <functional>
#include <map>
#include <utility>

// CSVの「種類＋番号」と処理を結び付ける対応表です。モデルやSceneは所有しません。
// SceneやFactoryが用途に合う処理を登録し、MapChipFieldが読んだChipを渡して使います。
class MapChipRegistry
{
public:
	using Handler = std::function<void(const MapChipField::Chip&)>;

	// 同じ種類・番号の再登録は上書きします。空の処理は登録せずfalseを返します。
	// 参照を捕まえる処理を登録する場合、その参照先はRunの終了まで生存させてください。
	bool Register(MapChipType type, uint32_t subId, Handler handler);
	// 一マスに登録済みの処理だけを実行します。未登録なら何もせずfalseを返します。
	// trueは処理を呼んだ意味で、ゲームオブジェクトの生成成功を保証する値ではありません。
	bool Run(const MapChipField::Chip& chip) const;

private:
	std::map<std::pair<MapChipType, uint32_t>, Handler> handlers_;
};
