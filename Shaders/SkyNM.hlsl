#include "CommonIncludeNM.hlsl"

struct vIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct vOut
{
    float4 pos : SV_Position;
    float3 cubeUV : POSITION;
};

vOut vert(vIn i)
{
    vOut o = (vOut) 0.0;
    o.cubeUV = i.pos;
    float4 posW = mul(float4(i.pos, 1.0), Model);
    posW.xyz += EyePosW; // skydome to cam
    o.pos = mul(posW, VP).xyww; // w div w we'll get 1 - far clip plane

    return o;
}

float4 frag(vOut i) : SV_Target
{
    return _cubeMap.Sample(_samAnisotropicWrap, i.cubeUV);
}