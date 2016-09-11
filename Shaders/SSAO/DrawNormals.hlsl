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
    float4 posW = mul(float4(i.pos, 1.0f), Model);
    o.pos = mul(posW, VP);
    o.normalW = mul(i.normalL, (float3x3) Model);
    o.tangentW = mul(i.tangent, (float3x3) Model);

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
    uint normalMapIndex = matData.NormalMapIndex;
    
    diffuseAlbedo *= _textureMaps[diffuseTexIndex].Sample(_samAnisotropicWrap, i.uv);
   
#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1f);
#endif
    i.normalW = normalize(i.normalW);

    float3 normalV = mul(i.normalW, (float3x3) View);
    return float4(normalV, 0.0f);
}
