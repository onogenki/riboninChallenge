#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "Matrix4x4.h"
#include "Vector4.h"
#include "Vector2.h"
#include "Vector3.h"

class DirectXCommon;
class Camera;
class Object3dCommon;

class SkyBox {
public:
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Matrix4x4 WorldInverseTranspose;
    };

    struct Material {
        Vector4 color;
        int32_t enableLighting;
        Matrix4x4 uvTransform;
        float shininess;
    };

    void Initialize(DirectXCommon* dxCommon, Camera* camera);
    void Update();
    void Draw();
    void SetTexture(const std::string& filePath) { texture_ = filePath; }//キューブマップのDDSのパスを指定
    void CreateRootSignature();

private:
    DirectXCommon* dxCommon_ = nullptr;
    Camera* camera_ = nullptr;
    Object3dCommon* object3dCommon_ = nullptr;

    // SkyBox専用の頂点・インデックスリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
    TransformationMatrix* transformationData_ = nullptr;
    // 座標変換用のリソース（WVP行列用）
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    std::string texture_;
};