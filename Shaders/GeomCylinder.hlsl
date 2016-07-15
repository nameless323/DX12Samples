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

float4 vert(float3 pos : POSITION) : SV_Position
{
    float4 posW = mul(float4(pos, 1.0), Model);
    return mul(posW, VP);
}

float4 frag(float4 pos : SV_Position) : SV_Target
{
    return 1.0;
}