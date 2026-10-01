#pragma once
#include "DirectXCommon.h"

//3Dモデル共通部
class ModelCommon
{
public:
	void Initialize(DirectXCommon* dxCommon);


	DirectXCommon* GetDxCommon() const { return dxCommon_; };

	private:
		DirectXCommon* dxCommon_;

};

