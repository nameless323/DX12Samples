#include "SkinnedAnimFrameResource.h"

SkinnedAnimFrameResource::SkinnedAnimFrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT skinnedObjectCount, UINT materialCount)
{
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
    MaterialBuffer = std::make_unique<UploadBuffer<MaterialData>>(device, materialCount, false);
    ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
    SSAOCB = std::make_unique<UploadBuffer<SSAOConstants>>(device, 1, true);
    SkinnedCB = std::make_unique<UploadBuffer<SkinnedConstants>>(device, skinnedObjectCount, true);
}

SkinnedAnimFrameResource::~SkinnedAnimFrameResource()
{
}
