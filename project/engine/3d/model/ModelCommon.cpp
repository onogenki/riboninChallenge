#include "ModelCommon.h"

namespace Engine
{

// ModelManagerが共通のDirectX操作窓口を一度渡し、後続のModel生成で使えるようにします。
void ModelCommon::Initialize(Engine::DirectXCommon* dxCommon)
{
	dxCommon_ = dxCommon;
}

}
