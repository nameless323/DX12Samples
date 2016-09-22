#pragma once
#include "SkinnedData.h"

struct SkinnedModelInstance
{
    SkinnedData* SkinnedInfo = nullptr;
    std::vector<DirectX::XMFLOAT4X4> FinalTransforms;
    std::string ClipName;

    float TimePos = 0.0f;

    void UpdateSkinnedAnimation(float dt)
    {
        TimePos += dt;

        if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
            TimePos = 0.0f;
        SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);
    }
};
