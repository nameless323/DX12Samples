#pragma once

#include <DirectXMath.h>
#include <DirectXPackedVector.h>

#include "../../../Core/MathHelper.h"
#include "../../../Core/D3DUtil.h"

namespace DX12Samples
{
struct WavesRenderItem
{
public:
    static const int NumFrameResources = 3;
    WavesRenderItem() = default;

    DirectX::XMFLOAT4X4 Model = MathHelper::Identity4x4();
    int NumFramesDirty = NumFrameResources;
    UINT ObjCBIndex = -1;

    MeshGeometry* Geo = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
    Opaque = 0,
    Count
};
}
