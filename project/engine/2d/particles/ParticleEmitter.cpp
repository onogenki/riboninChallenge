#include "ParticleEmitter.h"
#include "ParticleManager.h"

ParticleEmitter::ParticleEmitter(const std::string& name, const Transform& transform, uint32_t count, float frequency,bool receivesWind)
    : name_(name), transform_(transform), count_(count), frequency_(frequency), frequencyTime_(0.0f), receivesWind_(receivesWind)
{
    // コンストラクタで引数を受け取り、初期化リストでメンバ変数に書き込む
}

void ParticleEmitter::Update()
{
	if (ParticleManager::GetInstance()->IsReturning()) {
		return;
	}
    //1フレームの進む時間
    const float kDeltaTime = 1.0f / 60.0f;
    // 時刻を進める
    frequencyTime_ += kDeltaTime;

    // 発生頻度より大きいなら発生
    while (frequencyTime_ >= frequency_)
    {
        // 発生
        Emit();

        // 発生した分だけ時間を引き、余計に過ぎた時間がfrequencyTime_に残る(whileの理由)
        frequencyTime_ -= frequency_;
    }

}

void ParticleEmitter::Emit()
{
    // 設定値に従ってParticleManagerのEmitを呼び出す
    ParticleManager::GetInstance()->Emit(name_, transform_.translate, count_, receivesWind_, transform_.scale.x);
}
