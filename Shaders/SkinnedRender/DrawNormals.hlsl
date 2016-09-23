#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 0
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif


#include "CommonInclude.hlsl"

struct vIn
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 normalL : NORMAL;
    float3 tangent : TANGENT;
#ifdef SKINNED
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices : BONEINDICES;
#endif
};

struct vOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
    float3 normalW : NORMAL;
    float3 tangentW : TANGENT;
};

vOut vert(vIn i)
{
    vOut o = (vOut) 0.0;


    MaterialData matData = _materialData[MaterialIndex];

#ifdef SKINNED
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    weights[0] = i.BoneWeights.x;
    weights[1] = i.BoneWeights.y;
    weights[2] = i.BoneWeights.z;
    weights[3] = 1.0f - weights[0] - weights[1] - weights[2];

    float3 posL = float3(0.0f, 0.0f, 0.0f);
    float3 normalL = float3(0.0f, 0.0f, 0.0f);
    float3 tangentL = float3(0.0f, 0.0f, 0.0f);
    for (int j = 0; j < 4; j++)
    {
        posL += weights[j] * mul(float4(i.pos, 1.0f), BoneTransforms[i.BoneIndices[j]]).xyz;
        normalL += weights[j] * mul(i.normalL, (float3x3) BoneTransforms[i.BoneIndices[j]]);
        tangentL += weights[j] * mul(i.tangent.xyz, (float3x3) BoneTransforms[i.BoneIndices[J]]);
    }
    i.pos = posL;
    i.normalL = normalL;
    i.tangent.xyz = tangentL;
#endif
        o.normalW = mul(i.normalL, (float3x3) Model);
    o.tangentW = mul(i.tangent, (float3x3) Model);

    float4 posW = mul(float4(i.pos, 1.0f), Model);
    o.pos = mul(posW, VP);

    float4 uv = mul(float4(i.uv, 0.0f, 1.0f), TexTransform);

    o.uv = mul(uv, matData.MatTransform).xy;

    return o;
}

float4 frag(vOut i) : SV_Target
{
    MaterialData matData = _materialData[MaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    uint diffuseTexIndex = matData.DiffuseMapIndex;
    uint normalMapIndex = matData.NormalMapIndex;
    
    diffuseAlbedo *= _textureMaps[diffuseTexIndex].Sample(_samAnisotropicWrap, i.uv);
   
#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1f);
#endif
    i.normalW = normalize(i.normalW);

    float3 normalV = mul(i.normalW, (float3x3) View);
    return float4(normalV, 0.0f);
}
