#include "GameSceneRegistry.h"

#include "SceneFactory.h"
#include <vector>

namespace {
std::vector<GameSceneRegistry::Registrar>& GetRegistrars()
{
	static std::vector<GameSceneRegistry::Registrar> registrars;
	return registrars;
}
}

// game側から渡された登録処理を保存します。
void GameSceneRegistry::Register(Registrar registrar)
{
	GetRegistrars().push_back(std::move(registrar));
}

// 現在リンクされているgame側のSceneだけを、Factoryへ反映します。
void GameSceneRegistry::Apply(SceneFactory& sceneFactory)
{
	for (const Registrar& registrar : GetRegistrars()) {
		registrar(sceneFactory);
	}
}
