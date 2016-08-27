#pragma once

#include "../../Core/D3DUtil.h"
#include "../../Core/UploadBuffer.h"

struct NormalMappingFrameResource
{
public:
    static const UINT NumFrameResources = 3;

    struct ObjectConstants
    {
        DirectX::XMFLOAT4X4 Model = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
        UINT MaterialIndex;
        UINT Pad0;
        UINT Pad1;
        UINT Pad2;
    };

    struct PassConstants
    {
        DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 VP = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 InvVP = MathHelper::Identity4x4();
        DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
        float cbPerObjectPad1 = 0.0f;
        DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
        DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0, 0.0f };
        float NearZ = 0.0f;
        float FarZ = 0.0f;
        float TotalTime = 0.0f;
        float DeltaTime = 0.0f;

        DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

        Light Lights[MaxLights];
    };

    struct MaterialData
    {
        DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
        float Roughness = 64.0f;

        DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

        UINT DiffuseMapIndex = 0;
        UINT NormalMapIndex = 0;
        UINT Pad1;
        UINT Pad2;
    };

    struct Vertex
    {
        DirectX::XMFLOAT3 Pos;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT2 Uv;
        DirectX::XMFLOAT3 Tangent;
    };

    NormalMappingFrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
    NormalMappingFrameResource(const NormalMappingFrameResource& rhs) = delete;
    NormalMappingFrameResource& operator= (const NormalMappingFrameResource& rhs) = delete;
    ~NormalMappingFrameResource();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

    std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;

    UINT64 Fence = 0;
};