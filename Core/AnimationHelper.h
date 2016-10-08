//
// Helpers for animations.
//

#pragma once

#include "D3DUtil.h"

namespace DX12Samples
{
// Describes single keyframe.
struct Keyframe
{
    Keyframe();
    ~Keyframe();

    float TimePos;
    DirectX::XMFLOAT3 Translation;
    DirectX::XMFLOAT3 Scale;
    DirectX::XMFLOAT4 RotationQuat;
};

struct BoneAnimation
{
    /**
     * \brief Get animation start time (time of the first keyframe).
     */
    float GetStartTime() const;
    /**
     * \brief Get animation end time (time of the last keyframe).
     */
    float GetEndTime() const;
    /**
     * \brief Get transformation matrix at time.
     * \param t animation time.
     * \M ref to resulting matrix. Funciton will store result here.
     */
    void Interpolate(float t, DirectX::XMFLOAT4X4& M) const;

    std::vector<Keyframe> Keyframes;
};
}