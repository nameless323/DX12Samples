//
// Describes data which has to be set at each frame to GPU.
//

#pragma once

#include "../../Core/D3DUtil.h"
#include "../../Core/MathHelper.h"
#include "../../Core/UploadBuffer.h"
#include "../Shapes/ShapesFrameResource.h"

namespace DX12Samples
{
struct WavesFrameResource
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

    WavesFrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT waveVertCount);
    WavesFrameResource(const ShapesFrameResource& rhs) = delete;
    WavesFrameResource& operator= (const WavesFrameResource& rhs) = delete;
    ~WavesFrameResource();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;
    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

    std::unique_ptr<UploadBuffer<Vertex>> WavesVB = nullptr;

    UINT64 Fence = 0;
};
}