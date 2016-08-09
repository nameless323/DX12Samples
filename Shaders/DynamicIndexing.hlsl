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

SamplerState _samPointWrap : register(s0);
SamplerState _samPointClamp : register(s1);
SamplerState _samLinearWrap : register(s2);
SamplerState _samLinearClamp : register(s3);
SamplerState _samAnisotropicWrap : register(s4);
SamplerState _samAnisotropicClamp : register(s5);

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
    uint DiffuseMapIndex;
    uint MatPad0;
    uint MatPad1;
    uint MatPad2;
};

Texture2D _diffuseMap[4] : register(t0);
StructuredBuffer<MaterialData> _materialData : register(t0, space1);

cbuffer cbPerObject : register(b0)
{
    float4x4 Model;
    float4x4 TexTransform;
    uint MaterialIndex;
    uint ObjPad0;
    uint ObjPad1;
    uint ObjPad2;
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
    vOut o = (vOut) 0.0;
    float4 posW = mul(float4(i.pos, 1.0f), Model);
    o.posW = posW.xyz;
    o.normalW = mul(i.normal, (float3x3) Model);
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
    float3 fresnelR0 = matData.FresnelR0;
    float roughness = matData.Roughness;
    uint diffuseTexIndex = matData.DiffuseMapIndex;

    diffuseAlbedo *= _diffuseMap[diffuseTexIndex].Sample(_samAnisotropicWrap, i.uv);

    i.normalW = normalize(i.normalW);
    float3 toEyeW = normalize(EyePosW - i.posW);

    float4 ambient = AmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - roughness;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float3 shadowFactor = 1.0;
    float4 directLight = ComputeLighting(Lights, mat, i.posW, i.normalW, toEyeW, shadowFactor);
    float4 litColor = ambient + directLight;
    litColor.a = diffuseAlbedo.a;

    return litColor;
}