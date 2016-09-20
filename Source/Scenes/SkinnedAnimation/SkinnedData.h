#pragma once

#include "../../Core/D3DUtil.h"
#include "../../../Core/AnimationHelper.h"

struct AnimationClip
{
    float GetClipStartTime() const;
    float GetClipEndTime() const;

    void Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms) const;
    std::vector<BoneAnimation> BoneAnimations;
};

class SkinnedData
{
public:
    UINT BoneCount() const;

    float GetClipStartTime(const std::string& clipName) const;
    float GetClipEndTime(const std::string& clipName) const;

    void Set(std::vector<int>& boneHierarchy, std::vector<DirectX::XMFLOAT4X4>& boneOffsets, std::unordered_map<std::string, AnimationClip>& animations);

    void GetFinalTransforms(const std::string& clipName, float timePos, std::vector<DirectX::XMFLOAT4X4>& finalTransforms) const;
private:
    std::vector<int> _boneHierarchy;
    std::vector<DirectX::XMFLOAT4X4> _boneOffsets;
    std::unordered_map<std::string, AnimationClip> _animations;
};