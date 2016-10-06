//
// Describes data which nesessary to render single item in world.
//

#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>

#include "../../Core/MathHelper.h"
#include "../../Core/D3DUtil.h"
#include "FrameResourceUnfogged.h"
#include "../Scenes/SkinnedAnimation/SkinnedModelInstance.h"

struct RenderItem
{
    RenderItem() = default;

    DirectX::XMFLOAT4X4 Model = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    int NumFramesDirty = FrameResourceUnfogged::NumFrameResources;
    UINT ObjCBIndex = -1;
    Material* Mat = nullptr;
    MeshGeometry* Geo = nullptr;
    DirectX::XMFLOAT2 DisplacementMapTexelSize = { 1.0f, 1.0f };
    float GridSpatialStep = 1.0f;
    
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
    bool Visible = true;

    DirectX::BoundingBox Bounds;

    UINT SkinnedCBIndex = -1;
    SkinnedModelInstance* SkinnedModelInst = nullptr;

    enum class RenderLayer : int
    {
        Opaque = 0,
        Transparent,
        AlphaTested,
        AlphaTestedTreeSprites,
        Mirrors,
        Reflected,
        Shadow,
        GpuWaves,
        Sky,
        Highlight,
        OpaqueDynamicReflectors,
        SkinnedOpaque,
        Debug,
        Count
    };
};
