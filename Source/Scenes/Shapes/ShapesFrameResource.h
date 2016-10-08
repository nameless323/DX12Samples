#pragma once

#include "../../Core/D3DUtil.h"
#include "../../Core/MathHelper.h"
#include "../../Core/UploadBuffer.h"

namespace DX12Samples
{
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
};

struct ShapesFrameResource
{
public:
    struct Vertex
    {
        DirectX::XMFLOAT3 Pos;
        DirectX::XMFLOAT4 Color;
    };
    struct ObjectConstants
    {
        DirectX::XMFLOAT4X4 Model = MathHelper::Identity4x4();
    };

    ShapesFrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
    ShapesFrameResource(const ShapesFrameResource& rhs) = delete;
    ShapesFrameResource& operator= (const ShapesFrameResource& rhs) = delete;
    ~ShapesFrameResource();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

    UINT64 Fence = 0;   
};
}