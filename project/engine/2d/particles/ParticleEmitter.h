#pragma once
#include <string>
#include "MyMath.h"
#include "Object3d.h"

class ParticleEmitter
{
public:

    // ほとんどのメンバ変数を引数として受け取り、メンバ変数に書き込む
    ParticleEmitter(const std::string& name, const Transform& transform, uint32_t count, float frequency,bool receivesWind);

    // 更新処理
    void Update();

    // パーティクルの発生
    void Emit();

    // エミッタの位置などを後から変えたいとき
    void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; }
    void SetCount(uint32_t count) { count_ = count; }
    void SetFrequency(float frequency) { frequency_ = frequency; }
    void SetReceivesWind(bool receivesWind) { receivesWind_ = receivesWind; }
    const Transform& GetTransform() const { return transform_; }
    uint32_t GetCount() const { return count_; }
    float GetFrequency() const { return frequency_; }
    bool GetReceivesWind() const { return receivesWind_; }

private:

    std::string name_;   // 発生させるパーティクルグループ名
    Transform transform_;
    uint32_t count_;     // 1回あたりの発生数
    float frequency_;    // 発生頻度（何秒ごとに発生させるか）
    float frequencyTime_;// 時間
    bool receivesWind_;//風に吹かれてるか

};

