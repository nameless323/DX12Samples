#include "LightingUtil.hlsl"

cbuffer ObjectCB : register(b0)
{
    float4x4 Model;
    float4x4 TexTransform;
};

cbuffer PassCB : register(b1)
{
    float4x4 View;
    float4x4 InvView;
    float4x4 Proj;
    float4x4 InvProj;
    float4x4 VP;
    float4x4 InvVP;
    float3 EyePosW;
    float cbPerObjectPad1;
    float2 RenderTargetSize;
    float2 InvRenderTargetSize;
    float NearZ;
    float FarZ;
    float TotalTime;
    float DeltaTime;
    float4 AmbientLight;

    Light Lights[MaxLights];
};

struct vOut
{
    float3 pos : POSITION;
};

struct gOut
{
    float4 pos : SV_Position;
};

vOut vert(float3 pos : POSITION)
{
    vOut o;
    o.pos = pos;
    return o;
}

[maxvertexcount(4)]
void geom(line vOut i[2], inout TriangleStream<gOut> o)
{
    float3 v1 = i[0].pos;
    float3 v2 = i[0].pos + float3(0, 1, 0);
    float3 v3 = i[1].pos;
    float3 v4 = i[1].pos + float3(0, 1, 0);
    gOut outV;
    outV.pos = mul(float4(v1, 1.0), Model);
    outV.pos = mul(outV.pos, VP);
    o.Append(outV);

    outV.pos = mul(float4(v2, 1.0), Model);
    outV.pos = mul(outV.pos, VP);
    o.Append(outV);

    outV.pos = mul(float4(v3, 1.0), Model);
    outV.pos = mul(outV.pos, VP);
    o.Append(outV);

    outV.pos = mul(float4(v4, 1.0), Model);
    outV.pos = mul(outV.pos, VP);
    o.Append(outV);
}

float4 frag(gOut i) : SV_Target
{
    return 1.0;
}