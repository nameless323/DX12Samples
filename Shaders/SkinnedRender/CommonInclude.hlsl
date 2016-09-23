#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#include "../LightingUtil.hlsl"

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
    uint DiffuseMapIndex;
    uint NormalMapIndex;
    uint MatPad1;
    uint MatPad2;
};

TextureCube _cubeMap : register(t0);
Texture2D _shadowMap : register(t1);
Texture2D _ssaoMap : register(t2);

Texture2D _textureMaps[48] : register(t3);

StructuredBuffer<MaterialData> _materialData : register(t0, space1);

SamplerState _samPointWrap : register(s0);
SamplerState _samPointClamp : register(s1);
SamplerState _samLinearWrap : register(s2);
SamplerState _samLinearClamp : register(s3);
SamplerState _samAnisotropicWrap : register(s4);
SamplerState _samAnisotropicClamp : register(s5);
SamplerComparisonState _samShadow : register(s6);

cbuffer cbPerObject : register(b0)
{
    float4x4 Model;
    float4x4 TexTransform;
    uint MaterialIndex;
    uint ObjPad0;
    uint ObjPad1;
    uint ObjPad2;
};

cbuffer cbSkinned : register(b1)
{
    float4x4 BoneTransforms[96];
};

cbuffer cbPass : register(b2)
{
    float4x4 View;
    float4x4 InvView;
    float4x4 Proj;
    float4x4 InvProj;
    float4x4 VP;
    float4x4 InvVP;
    float4x4 ViewProjTex;
    float4x4 ShadowTransform;
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

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    float3 normalT = 2.0f * normalMapSample - 1.0f;
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);

    float3x3 TBN = float3x3(T, B, N);
    float3 bumpedNormalW = mul(normalT, TBN);

    return bumpedNormalW;
}

float CalcShadowFactor(float4 shadowPosH)
{
    shadowPosH.xyz /= shadowPosH.w;
    float depth = shadowPosH.z;

    uint width, height, numMips;
    _shadowMap.GetDimensions(0, width, height, numMips);

    float dx = 1.0f / (float) width;
    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };
    [unroll]
    for (int i = 0; i < 9; i++)
    {
        percentLit += _shadowMap.SampleCmpLevelZero(_samShadow, shadowPosH.xy + offsets[i], depth).r;
    }
    return percentLit / 9.0f;
}