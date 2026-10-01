#pragma once

#include"BaseScene.h"
#include<string>
#include<vector>
#include<memory>

//シーン工場
class AbstractSceneFactory
{
public:

	//仮想デストラクタ
	virtual ~AbstractSceneFactory() = default;
	//シーン生成
	virtual std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) = 0;
	// 登録されているシーン名一覧
	virtual const std::vector<std::string>& GetSceneNames() const = 0;

};
