#include "SkyBox.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "MyMath.h"
#include"Camera.h"
#include "Object3dCommon.h"
#include <cassert>

using namespace MyMath;

void SkyBox::Initialize(DirectXCommon* dxCommon, Camera* camera) {
    assert(dxCommon);
    assert(camera);

    // 引数でポインタを受け取ってメンバ変数に記録
    dxCommon_ = dxCommon;
    camera_ = camera;

    ID3D12Device* device = dxCommon_->GetDevice();

    const uint32_t vertexCount = 24;
    // 頂点バッファの生成が抜けていたのを追加
    vertexBuffer_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * vertexCount);

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * vertexCount;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // 適切な構造体ポインタ(VertexData*)としてMapする
    VertexData* vertexData = nullptr;
    vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

    // --- 頂点データの定義 ---
    // 右面 (X = 1.0f)
    vertexData[0].position = { 1.0f, 1.0f, 1.0f, 1.0f };
    vertexData[1].position = { 1.0f, 1.0f,-1.0f, 1.0f };
    vertexData[2].position = { 1.0f,-1.0f, 1.0f, 1.0f };
    vertexData[3].position = { 1.0f,-1.0f,-1.0f, 1.0f };

    // 左面 (X = -1.0f)
    vertexData[4].position = { -1.0f, 1.0f,-1.0f, 1.0f };
    vertexData[5].position = { -1.0f, 1.0f, 1.0f, 1.0f };
    vertexData[6].position = { -1.0f,-1.0f,-1.0f, 1.0f };
    vertexData[7].position = { -1.0f,-1.0f, 1.0f, 1.0f };

    // 上面 (Y = 1.0f)
    vertexData[8].position = { -1.0f,  1.0f,  1.0f, 1.0f };
    vertexData[9].position = { 1.0f,  1.0f,  1.0f, 1.0f };
    vertexData[10].position = { -1.0f,  1.0f, -1.0f, 1.0f };
    vertexData[11].position = { 1.0f,  1.0f, -1.0f, 1.0f };

    // 下面 (Y = -1.0f)
    vertexData[12].position = { -1.0f, -1.0f, -1.0f, 1.0f };
    vertexData[13].position = { 1.0f, -1.0f, -1.0f, 1.0f };
    vertexData[14].position = { -1.0f, -1.0f,  1.0f, 1.0f };
    vertexData[15].position = { 1.0f, -1.0f,  1.0f, 1.0f };

    // 前面 (Z = 1.0f)
    vertexData[16].position = { -1.0f,  1.0f,  1.0f, 1.0f };
    vertexData[17].position = { 1.0f,  1.0f,  1.0f, 1.0f };
    vertexData[18].position = { -1.0f, -1.0f,  1.0f, 1.0f };
    vertexData[19].position = { 1.0f, -1.0f,  1.0f, 1.0f };

    // 後面 (Z = -1.0f)
    vertexData[20].position = { 1.0f,  1.0f, -1.0f, 1.0f };
    vertexData[21].position = { -1.0f,  1.0f, -1.0f, 1.0f };
    vertexData[22].position = { 1.0f, -1.0f, -1.0f, 1.0f };
    vertexData[23].position = { -1.0f, -1.0f, -1.0f, 1.0f };

    // UVと法線はSkyBoxシェーダー内では使わないので、安全のためゼロクリアしておく
    for (uint32_t i = 0; i < vertexCount; ++i) {
        vertexData[i].texcoord = { 0.0f, 0.0f };
        vertexData[i].normal = { 0.0f, 0.0f, 0.0f };
    }

    vertexBuffer_->Unmap(0, nullptr);

    const uint32_t indexCount = 36;
    indexBuffer_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * indexCount);

    indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * indexCount;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    uint32_t* indexData = nullptr;
    indexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));

    // 各面を内側から見た時に「時計回り」になるようにインデックスを設定
    // 右面
    indexData[0] = 0;  indexData[1] = 1;  indexData[2] = 2;
    indexData[3] = 2;  indexData[4] = 1;  indexData[5] = 3;

    // 左面
    indexData[6] = 4;  indexData[7] = 5;  indexData[8] = 6;
    indexData[9] = 6;  indexData[10] = 5; indexData[11] = 7;

    // 上面の巻き順を時計回りに修正
    indexData[12] = 8;  indexData[13] = 10; indexData[14] = 9;
    indexData[15] = 10; indexData[16] = 11; indexData[17] = 9;

    // 下面の巻き順を時計回りに修正
    indexData[18] = 12; indexData[19] = 14; indexData[20] = 13;
    indexData[21] = 14; indexData[22] = 15; indexData[23] = 13;

    // 前面
    indexData[24] = 16; indexData[25] = 17; indexData[26] = 18;
    indexData[27] = 18; indexData[28] = 17; indexData[29] = 19;

    // 後面
    indexData[30] = 20; indexData[31] = 21; indexData[32] = 22;
    indexData[33] = 22; indexData[34] = 21; indexData[35] = 23;

    indexBuffer_->Unmap(0, nullptr);

    // 座標変換行列
    transformationResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
    transformationResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationData_));
    transformationData_->World = MakeIdentity4x4();
    transformationData_->WVP = MakeIdentity4x4();
    transformationData_->WorldInverseTranspose = MakeIdentity4x4();

    // マテリアル
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 白（テクスチャの色をそのまま出す）
    materialData_->enableLighting = 0;                  // ライティング無効
    materialData_->uvTransform = MakeIdentity4x4();
    materialData_->shininess = 0.0f;
}

void SkyBox::Update() {
    // SkyBoxは常にカメラと同じ位置に存在させる（平行移動を相殺するため）
    // スケールは適当（1.0fでOK、VSの.xywwトリックがあるためクリップ面に張り付く）
    Vector3 skyboxScale = {1.0f, 1.0f, 1.0f };
    Vector3 skyboxRotate = { 0.0f, 0.0f, 0.0f };
    Vector3 skyboxTranslate = camera_->GetTranslate(); // カメラの位置をそのまま設定

    Matrix4x4 worldMatrix = MakeAffineMatrix(skyboxScale, skyboxRotate, skyboxTranslate);
    const Matrix4x4& viewProjectionMatrix = camera_->GetViewProjectionMatrix();

    // 行列をマッピング先へ転送
    transformationData_->World = worldMatrix;
    transformationData_->WVP = Multiply(worldMatrix, viewProjectionMatrix);
    transformationData_->WorldInverseTranspose = Transpose(Inverse(worldMatrix));
}

void SkyBox::Draw() {
    // コマンドリストの取得
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    //シングルトンからインスタンスを直接取得する
    Object3dCommon* object3dCommon = Object3dCommon::GetInstance();
    assert(object3dCommon);

    //Skybox専用のパイプラインとシグネチャに切り替える
    commandList->SetGraphicsRootSignature(object3dCommon->GetSkyboxRootSignature());
    commandList->SetPipelineState(object3dCommon->GetSkyboxPipelineState());

    // ピクセルシェーダー側
    commandList->SetGraphicsRootConstantBufferView(0, transformationResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());

    // キューブマップテクスチャのバインド（レジスタ t0 にSRVハンドルをセット）
    TextureManager* textureManager = TextureManager::GetInstance();
    commandList->SetGraphicsRootDescriptorTable(2, textureManager->GetSrvHandleGPU(texture_));

    // VB, IBをセットして描画
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}