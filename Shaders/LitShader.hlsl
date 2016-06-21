#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 Model;
};

cbuffer cbMaterial : register(b1)
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
};

cbuffer cbPass : register(b2)
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

struct vIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct vOut
{
    float4 PosH : SV_Position;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
};

vOut vert(vIn i)
{
    vOut o = (vOut) 0.0f;
    float4 posW = mul(float4(i.pos, 1.0f), Model);
    o.PosW = posW.xyz;

    o.NormalW = mul(i.normal, (float3x3) Model);
    o.PosH = mul(posW, VP);

    return o;
}

float4 frag(vOut i) : SV_Target
{
    i.NormalW = normalize(i.NormalW);

    float3 toEyeW = normalize(EyePosW - i.PosW);
    float4 ambient = AmbientLight * DiffuseAlbedo;

    const float shininess = 1.0f - Roughness;
    Material mat = { DiffuseAlbedo, FresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(Lights, mat, i.PosW, i.NormalW, toEyeW, shadowFactor);
    float4 litColor = ambient + directLight;
    litColor.a = DiffuseAlbedo.a;
    return litColor;
}