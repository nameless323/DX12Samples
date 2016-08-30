#include "CommonIncludeSM.hlsl"

struct vIn
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct vOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

vOut vert(vIn i)
{
    vOut o = (vOut) 0.0;
    float4 posW = mul(float4(i.pos, 1.0f), Model);
    o.pos = mul(posW, VP);

    float4 uv = mul(float4(i.uv, 0.0f, 1.0f), TexTransform);

    MaterialData matData = _materialData[MaterialIndex];
    o.uv = mul(uv, matData.MatTransform).xy;

    return o;
}

float4 frag(vOut i) : SV_Target
{
    MaterialData matData = _materialData[MaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    uint diffuseTexIndex = matData.DiffuseMapIndex;
    
    diffuseAlbedo *= _textureMaps[diffuseTexIndex].Sample(_samAnisotropicWrap, i.uv);
   
#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1f);
#endif
    return 0;
}