#include "CommonInclude.hlsl"

struct vIn
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
#ifdef SKINNED
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices : BONEINDICES;
#endif
};

struct vOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

vOut vert(vIn i)
{
    vOut o = (vOut) 0.0;
#ifdef SKINNED
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = i.BoneWeights.x;
    weights[1] = i.BoneWeights.y;
    weights[2] = i.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

    float3 posL = float3(0.0f, 0.0f, 0.0f);
    for (int j = 0; j < 4; j++)
    {
        posL += weights[j] * mul(float4(i.pos, 1.0f), BoneTransforms[i.BoneIndices[j]]).xyz;
    }
    i.pos = posL;
#endif
    float4 posW = mul(float4(i.pos, 1.0f), Model);
    o.pos = mul(posW, VP);

    float4 uv = mul(float4(i.uv, 0.0f, 1.0f), TexTransform);

    MaterialData matData = _materialData[MaterialIndex];
    o.uv = mul(uv, matData.MatTransform).xy;

    return o;
}

void frag(vOut i)
{
    MaterialData matData = _materialData[MaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    uint diffuseTexIndex = matData.DiffuseMapIndex;
    
    diffuseAlbedo *= _textureMaps[diffuseTexIndex].Sample(_samAnisotropicWrap, i.uv);
   
#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1f);
#endif
}
