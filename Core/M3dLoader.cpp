#include "M3dLoader.h"

namespace DX12Samples
{
using namespace DirectX;

bool M3dLoader::LoadM3d(const std::string& filename, std::vector<Vertex>& vertices, std::vector<USHORT>& indices, std::vector<Subset>& subsets, std::vector<M3dMaterial>& mats)
{
    std::ifstream fin(filename);
    UINT numMaterials = 0;
    UINT numVertices = 0;
    UINT numTriangles = 0;
    UINT numBones = 0;
    UINT numAnimationClips = 0;

    std::string ignore;

    if (fin)
    {
        fin >> ignore;
        fin >> ignore >> numMaterials;
        fin >> ignore >> numVertices;
        fin >> ignore >> numTriangles;
        fin >> ignore >> numBones;
        fin >> ignore >> numAnimationClips;

        ReadMaterials(fin, numMaterials, mats);
        ReadSubsetTable(fin, numMaterials, subsets);
        ReadVertices(fin, numVertices, vertices);
        ReadTriangles(fin, numTriangles, indices);
        return true;
    }
    return false;
}

bool M3dLoader::LoadM3d(const std::string& filename, std::vector<SkinnedVertex>& vertices, std::vector<USHORT>& indices, std::vector<Subset>& subsets, std::vector<M3dMaterial>& mats, SkinnedData& skinInfo)
{
    std::ifstream fin(filename);
    UINT numMaterials = 0;
    UINT numVertices = 0;
    UINT numTriangles = 0;
    UINT numBones = 0;
    UINT numAnimationClips = 0;

    std::string ignore;

    if (fin)
    {
        fin >> ignore;
        fin >> ignore >> numMaterials;
        fin >> ignore >> numVertices;
        fin >> ignore >> numTriangles;
        fin >> ignore >> numBones;
        fin >> ignore >> numAnimationClips;

        std::vector<XMFLOAT4X4> boneOffsets;
        std::vector<int> boneIndexToParentIndex;
        std::unordered_map<std::string, AnimationClip> animations;

        ReadMaterials(fin, numMaterials, mats);
        ReadSubsetTable(fin, numMaterials, subsets);
        ReadSkinnedVertices(fin, numVertices, vertices);
        ReadTriangles(fin, numTriangles, indices);
        ReadBoneOffsets(fin, numBones, boneOffsets);
        ReadBoneHierarchy(fin, numBones, boneIndexToParentIndex);
        ReadAnimationClips(fin, numBones, numAnimationClips, animations);

        skinInfo.Set(boneIndexToParentIndex, boneOffsets, animations);
        return true;
    }
    return false;
}

void M3dLoader::ReadMaterials(std::ifstream& fin, UINT numMaterials, std::vector<M3dMaterial>& mats)
{
    std::string ignore;
    mats.resize(numMaterials);

    std::string diffuseMapName;
    std::string normalMapName;

    fin >> ignore;
    for (UINT i = 0; i < numMaterials; ++i)
    {
        fin >> ignore >> mats[i].Name;
        fin >> ignore >> mats[i].DiffuseAlbedo.x >> mats[i].DiffuseAlbedo.y >> mats[i].DiffuseAlbedo.z;
        fin >> ignore >> mats[i].FresnelR0.x >> mats[i].FresnelR0.y >> mats[i].FresnelR0.z;
        fin >> ignore >> mats[i].Roughness;
        fin >> ignore >> mats[i].AlphaClip;
        fin >> ignore >> mats[i].MaterialTypeName;
        fin >> ignore >> mats[i].DiffuseMapName;
        fin >> ignore >> mats[i].NormalMapName;
    }
}

void M3dLoader::ReadSubsetTable(std::ifstream& fin, UINT numSubsets, std::vector<Subset>& subsets)
{
    std::string ignore;
    subsets.resize(numSubsets);

    fin >> ignore;
    for (UINT i = 0; i < numSubsets; ++i)
    {
        fin >> ignore >> subsets[i].Id;
        fin >> ignore >> subsets[i].VertexStart;
        fin >> ignore >> subsets[i].VertexCount;
        fin >> ignore >> subsets[i].FaceStart;
        fin >> ignore >> subsets[i].FaceCount;
    }
}

void M3dLoader::ReadVertices(std::ifstream& fin, UINT numVertices, std::vector<Vertex>& vertices)
{
    std::string ignore;
    vertices.resize(numVertices);

    fin >> ignore; // vertices header text
    for (UINT i = 0; i < numVertices; ++i)
    {
        fin >> ignore >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
        fin >> ignore >> vertices[i].TangentU.x >> vertices[i].TangentU.y >> vertices[i].TangentU.z >> vertices[i].TangentU.w;
        fin >> ignore >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
        fin >> ignore >> vertices[i].Uv.x >> vertices[i].Uv.y;
    }
}

void M3dLoader::ReadSkinnedVertices(std::ifstream& fin, UINT numVertices, std::vector<SkinnedVertex>& vertices)
{
    std::string ignore;
    vertices.resize(numVertices);

    fin >> ignore; // vertices header text
    int boneIndices[4];
    float weights[4];
    for (UINT i = 0; i < numVertices; ++i)
    {
        float blah;
        fin >> ignore >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
        fin >> ignore >> vertices[i].TangentU.x >> vertices[i].TangentU.y >> vertices[i].TangentU.z >> blah /*vertices[i].TangentU.w*/;
        fin >> ignore >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
        fin >> ignore >> vertices[i].Uv.x >> vertices[i].Uv.y;
        fin >> ignore >> weights[0] >> weights[1] >> weights[2] >> weights[3];
        fin >> ignore >> boneIndices[0] >> boneIndices[1] >> boneIndices[2] >> boneIndices[3];

        vertices[i].BoneWeights.x = weights[0];
        vertices[i].BoneWeights.y = weights[1];
        vertices[i].BoneWeights.z = weights[2];

        vertices[i].BoneIndices[0] = (BYTE)boneIndices[0];
        vertices[i].BoneIndices[1] = (BYTE)boneIndices[1];
        vertices[i].BoneIndices[2] = (BYTE)boneIndices[2];
        vertices[i].BoneIndices[3] = (BYTE)boneIndices[3];
    }
}

void M3dLoader::ReadTriangles(std::ifstream& fin, UINT numTriangles, std::vector<USHORT>& indices)
{
    std::string ignore;
    indices.resize(numTriangles * 3);

    fin >> ignore;
    for (UINT i = 0; i < numTriangles; ++i)
    {
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    }
}

void M3dLoader::ReadBoneOffsets(std::ifstream& fin, UINT numBones, std::vector<DirectX::XMFLOAT4X4>& boneOffsets)
{
    std::string ignore;
    boneOffsets.resize(numBones);

    fin >> ignore;
    for (UINT i = 0; i < numBones; ++i)
    {
        fin >> ignore >>
            boneOffsets[i](0, 0) >> boneOffsets[i](0, 1) >> boneOffsets[i](0, 2) >> boneOffsets[i](0, 3) >>
            boneOffsets[i](1, 0) >> boneOffsets[i](1, 1) >> boneOffsets[i](1, 2) >> boneOffsets[i](1, 3) >>
            boneOffsets[i](2, 0) >> boneOffsets[i](2, 1) >> boneOffsets[i](2, 2) >> boneOffsets[i](2, 3) >>
            boneOffsets[i](3, 0) >> boneOffsets[i](3, 1) >> boneOffsets[i](3, 2) >> boneOffsets[i](3, 3);
    }
}

void M3dLoader::ReadBoneHierarchy(std::ifstream& fin, UINT numBones, std::vector<int>& boneIndexToParentIndex)
{
    std::string ignore;
    boneIndexToParentIndex.resize(numBones);

    fin >> ignore;
    for (UINT i = 0; i < numBones; ++i)
    {
        fin >> ignore >> boneIndexToParentIndex[i];
    }
}

void M3dLoader::ReadAnimationClips(std::ifstream& fin, UINT numBones, UINT numAnimationClips, std::unordered_map<std::string, AnimationClip>& animations)
{
    std::string ignore;
    fin >> ignore;
    for (UINT clipIndex = 0; clipIndex < numAnimationClips; ++clipIndex)
    {
        std::string clipName;
        fin >> ignore >> clipName;
        fin >> ignore;

        AnimationClip clip;
        clip.BoneAnimations.resize(numBones);

        for (UINT boneIndex = 0; boneIndex < numBones; ++boneIndex)
        {
            ReadBoneKeyframes(fin, numBones, clip.BoneAnimations[boneIndex]);
        }
        fin >> ignore;

        animations[clipName] = clip;
    }
}

void M3dLoader::ReadBoneKeyframes(std::ifstream& fin, UINT numBones, BoneAnimation& boneAnimation)
{
    std::string ignore;
    UINT numKeyframes = 0;
    fin >> ignore >> ignore >> numKeyframes;
    fin >> ignore;

    boneAnimation.Keyframes.resize(numKeyframes);
    for (UINT i = 0; i < numKeyframes; ++i)
    {
        float t = 0.0f;
        XMFLOAT3 p(0.0f, 0.0f, 0.0f);
        XMFLOAT3 s(1.0f, 1.0f, 1.0f);
        XMFLOAT4 q(0.0f, 0.0f, 0.0f, 1.0f);
        fin >> ignore >> t;
        fin >> ignore >> p.x >> p.y >> p.z;
        fin >> ignore >> s.x >> s.y >> s.z;
        fin >> ignore >> q.x >> q.y >> q.z >> q.w;

        boneAnimation.Keyframes[i].TimePos = t;
        boneAnimation.Keyframes[i].Translation = p;
        boneAnimation.Keyframes[i].Scale = s;
        boneAnimation.Keyframes[i].RotationQuat = q;
    }

    fin >> ignore;
}
}