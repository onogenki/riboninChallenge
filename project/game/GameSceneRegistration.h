#pragma once

class SceneFactory;

// ゲームで使うScene生成規則を、起動時に明示的にFactoryへ登録します。
void RegisterGameScenes(SceneFactory& sceneFactory);
