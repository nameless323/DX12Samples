//
// Describes data which has to be set at each frame to GPU.
//

#pragma once

#include "../../Core/D3DUtil.h"
#include "../../Core/UploadBuffer.h"

namespace DX12Samples
{
struct SkinnedAnimFrameResource
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

    struct SkinnedConstants
    {
        DirectX::XMFLOAT4X4 BoneTransforms[96];
    };

    struct PassConstants
    {
        DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 VP = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 InvVP = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 ViewProjTex = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();
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
        float Roughness = 0.5f;

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

    struct SkinnedVertex
    {
        DirectX::XMFLOAT3 Pos;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT2 Uv;
        DirectX::XMFLOAT3 Tangent;
        DirectX::XMFLOAT3 BoneWeights;
        BYTE BoneIndices[4];
    };

    struct SSAOConstants
    {
        DirectX::XMFLOAT4X4 Proj;
        DirectX::XMFLOAT4X4 InvProj;
        DirectX::XMFLOAT4X4 ProjTex;
        DirectX::XMFLOAT4 OffsetVectors[14];

        DirectX::XMFLOAT4 BlurWeights[3];

        DirectX::XMFLOAT2 InvRenderTartetSize = { 0.0f, 0.0f };

        float OcclusionRadius = 0.5f;
        float OcclusionFadeStart = 0.2f;
        float OcclusionFadeEnd = 2.0f;
        float SurfaceEpsilon = 0.05f;
    };

    SkinnedAnimFrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT skinnedObjectCount, UINT materialCount);
    SkinnedAnimFrameResource(const SkinnedAnimFrameResource& rhs) = delete;
    SkinnedAnimFrameResource& operator= (const SkinnedAnimFrameResource& rhs) = delete;
    ~SkinnedAnimFrameResource();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
    std::unique_ptr<UploadBuffer<SkinnedConstants>> SkinnedCB = nullptr;
    std::unique_ptr<UploadBuffer<SSAOConstants>> SSAOCB = nullptr;

    std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;

    UINT64 Fence = 0;
};
}