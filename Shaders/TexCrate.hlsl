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

Texture2D _diffuseMap : register(t0);
SamplerState _samplerLinear : register(s0);

cbuffer cbPerObject : register(b0)
{
    float4x4 Model;
    float4x4 TexTransform;
};

cbuffer cbPass : register(b1)
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

cbuffer cbMaterial : register(b2)
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
};

struct vIn
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct vOut
{
    float4 pos : SV_Position;
    float3 posW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
};

vOut vert(vIn i)
{
    vOut o;
    float4 posW = mul(float4(i.pos, 1.0f), Model);
    o.posW = posW.xyz;
    o.normalW = mul(i.normal, (float3x3) Model);
    o.pos = mul(posW, VP);
    float4 uv = mul(float4(i.uv, 0.0f, 1.0f), TexTransform);
    o.uv = mul(uv, MatTransform).xy;

    return o;
}

float4 frag(vOut i) : SV_Target
{
    float4 diffuseAlbedo = _diffuseMap.Sample(_samplerLinear, i.uv) * DiffuseAlbedo;

    i.normalW = normalize(i.normalW);
    float3 toEyeW = normalize(EyePosW - i.posW);

    float4 ambient = AmbientLight * DiffuseAlbedo;

    const float shininess = 1.0f - Roughness;
    Material mat = { DiffuseAlbedo, FresnelR0, shininess };
    float3 shadowFactor = 1.0;
    float4 directLight = ComputeLighting(Lights, mat, i.posW, i.normalW, toEyeW, shadowFactor);
    float4 litColor = ambient + directLight;
    litColor.a = DiffuseAlbedo.a;

    return litColor;
}