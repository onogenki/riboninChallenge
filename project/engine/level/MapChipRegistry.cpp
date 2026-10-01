#include "MapChipRegistry.h"

// B0とB1などを別の処理として登録し、未登録の番号へ勝手に代替処理を適用しません。
bool MapChipRegistry::Register(MapChipType type, uint32_t subId, Handler handler)
{
	if (type == MapChipType::Unknown || !handler) {
		return false;
	}
	handlers_.insert_or_assign({ type, subId }, std::move(handler));
	return true;
}

// 読込済みの一マスを対応表で検索し、生成などの具体的な仕事は登録者へ任せます。
bool MapChipRegistry::Run(const MapChipField::Chip& chip) const
{
	const auto found = handlers_.find({ MapChipField::GetType(chip), chip.subId });
	if (found == handlers_.end()) {
		return false;
	}
	found->second(chip);
	return true;
}
