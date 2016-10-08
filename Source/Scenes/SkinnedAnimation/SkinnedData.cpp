#include "SkinnedData.h"

namespace DX12Samples
{
using namespace DirectX;

float AnimationClip::GetClipStartTime() const
{
    float t = MathHelper::Infinity;
    for (UINT i = 0; i < BoneAnimations.size(); i++)
        t = MathHelper::Min(t, BoneAnimations[i].GetStartTime());
    return t;
}

float AnimationClip::GetClipEndTime() const
{
    float t = 0.0f;
    for (UINT i = 0; i < BoneAnimations.size(); i++)
        t = MathHelper::Max(t, BoneAnimations[i].GetEndTime());
    return t;
}

void AnimationClip::Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms) const
{
    for (UINT i = 0; i < BoneAnimations.size(); i++)
        BoneAnimations[i].Interpolate(t, boneTransforms[i]);
}

UINT SkinnedData::BoneCount() const
{
    return _boneHierarchy.size();
}

float SkinnedData::GetClipStartTime(const std::string& clipName) const
{
    auto clip = _animations.find(clipName);
    return clip->second.GetClipStartTime();
}

float SkinnedData::GetClipEndTime(const std::string& clipName) const
{
    auto clip = _animations.find(clipName);
    return clip->second.GetClipEndTime();
}

void SkinnedData::Set(std::vector<int>& boneHierarchy, std::vector<DirectX::XMFLOAT4X4>& boneOffsets, std::unordered_map<std::string, AnimationClip>& animations)
{
    _boneHierarchy = boneHierarchy;
    _boneOffsets = boneOffsets;
    _animations = animations;
}

void SkinnedData::GetFinalTransforms(const std::string& clipName, float timePos, std::vector<DirectX::XMFLOAT4X4>& finalTransforms) const
{
    UINT numBones = _boneOffsets.size();

    std::vector<XMFLOAT4X4> toParentTransforms(numBones);

    auto clip = _animations.find(clipName);
    clip->second.Interpolate(timePos, toParentTransforms);

    std::vector<XMFLOAT4X4> toRootTransforms(numBones);
    toRootTransforms[0] = toParentTransforms[0];

    for (UINT i = 1; i < numBones; i++)
    {
        XMMATRIX toParent = XMLoadFloat4x4(&toParentTransforms[i]);
        int parentIndex = _boneHierarchy[i];
        XMMATRIX parentToRoot = XMLoadFloat4x4(&toRootTransforms[parentIndex]);
        XMMATRIX toRoot = XMMatrixMultiply(toParent, parentToRoot);
        XMStoreFloat4x4(&toRootTransforms[i], toRoot);
    }
    for (UINT i = 0; i < numBones; i++)
    {
        XMMATRIX offset = XMLoadFloat4x4(&_boneOffsets[i]);
        XMMATRIX toRoot = XMLoadFloat4x4(&toRootTransforms[i]);
        XMMATRIX finalTransform = XMMatrixMultiply(offset, toRoot);
        XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(finalTransform));
    }
}
}