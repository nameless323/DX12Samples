#include "CommonIncludeSM.hlsl"

struct vIn
{
    float3 Pos : POSITION;
    float2 uv : TEXCOORD;
};

struct vOut
{
    float4 Pos : SV_Position;
    float2 uv : TEXCOORD;
};

vOut vert(vIn i)
{
    vOut o = (vOut) 0.0f;
    o.Pos = float4(i.Pos, 1.0f);
    o.uv = i.uv;
    return o;
}

float4 frag(vOut i) : SV_Target
{
    return float4(_shadowMap.Sample(_samLinearWrap, i.uv).rrr, 1.0);
}