//
// Render item description for instancing.
//

#pragma once

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include "InstancingFrameResource.h"
#include "../../Core/D3DUtil.h"

namespace DX12Samples
{
struct InstancingRenderItem
{
    InstancingRenderItem() = default;
    InstancingRenderItem(const InstancingRenderItem& rhs) = delete;

    DirectX::XMFLOAT4X4 Model = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

    int NumFramesDirty = 3;

    UINT ObjCBIndex = -1;
    Material* Mat = nullptr;
    MeshGeometry* Geo = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    DirectX::BoundingBox Bounds;
    std::vector<InstancingFrameResource::InstanceData> Instances;

    UINT IndexCount = 0;
    UINT InstanceCount = 0;
    UINT StartIndexLocation = 0;
    UINT BaseVertexLocation = 0;
};
}