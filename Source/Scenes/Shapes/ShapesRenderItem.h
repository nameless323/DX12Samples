#pragma once

#include "../../Core/D3DUtil.h"
#include "../../Core/MathHelper.h"

const int NumFrameResources = 3;

struct ShapesRenderItem
{
    ShapesRenderItem() = default;

    DirectX::XMFLOAT4X4 Model = MathHelper::Identity4x4();
    int NumFramesDirty = NumFrameResources;
    UINT ObjCBIndex = -1;
    MeshGeometry* Geo = nullptr;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};