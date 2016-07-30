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
Texture2D _displacementMap : register(t1);

SamplerState _samPointWrap : register(s0);
SamplerState _samPointClamp : register(s1);
SamplerState _samLinearWrap : register(s2);
SamplerState _samLinearClamp : register(s3);
SamplerState _samAnisotropicWrap : register(s4);
SamplerState _samAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
    float4x4 Model;
    float4x4 TexTransform;
    float2 DisplacementMapTexelSize;
    float GridSpatialStep;
    float cbPerObjectPad;
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

    float4 FogColor;
    float FogStart;
    float FogRange;
    float2 cbPerObjectPad2;

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
    vOut o = (vOut) 0.0;

#ifdef DISPLACEMENT_MAP
	i.pos.y += _displacementMap.SampleLevel(_samLinearWrap, i.uv, 1.0f).r;
	
	// Estimate normal using finite difference.
    float du = DisplacementMapTexelSize.x;
    float dv = DisplacementMapTexelSize.y;
	float l = _displacementMap.SampleLevel(_samPointClamp, i.uv -float2(du, 0.0f), 0.0f ).r;
    float r = _displacementMap.SampleLevel(_samPointClamp, i.uv + float2(du, 0.0f), 0.0f).r;
    float t = _displacementMap.SampleLevel(_samPointClamp, i.uv - float2(0.0f, dv), 0.0f).r;
    float b = _displacementMap.SampleLevel(_samPointClamp, i.uv + float2(0.0f, dv), 0.0f).r;
	i.normal = normalize( float3(-r+l, 2.0f*GridSpatialStep, b-t) );
	
#endif

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
    float4 diffuseAlbedo = _diffuseMap.Sample(_samAnisotropicWrap, i.uv) * DiffuseAlbedo;

#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1);
#endif

    i.normalW = normalize(i.normalW);
    float3 toEyeW = EyePosW - i.posW;
    float distToEye = length(toEyeW);
    toEyeW /= distToEye;

    float4 ambient = AmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - Roughness;
    Material mat = { diffuseAlbedo, FresnelR0, shininess };
    float3 shadowFactor = 1.0;
    float4 directLight = ComputeLighting(Lights, mat, i.posW, i.normalW, toEyeW, shadowFactor);
    float4 litColor = ambient + directLight;
    
#ifdef FOG
    float fogAmount = saturate((distToEye - FogStart) / FogRange);
    litColor = lerp(litColor, FogColor, fogAmount);
#endif
    litColor.a = diffuseAlbedo.a;

    return litColor;
}