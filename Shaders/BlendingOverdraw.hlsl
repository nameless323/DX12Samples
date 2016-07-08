#include "LightingUtil.hlsl"

Texture2D _diffuseMap : register(t0);
SamplerState _samPointWrap : register(s0);
SamplerState _samPointClamp : register(s1);
SamplerState _samLinearWrap : register(s2);
SamplerState _samLinearClamp : register(s3);
SamplerState _samAnisotropicWrap : register(s4);
SamplerState _samAnisotropicClamp : register(s5);

struct cbPerObject
{
    float4x4 Model;
    float4x4 TexTransform;
};
ConstantBuffer<cbPerObject> _cbPerObject : register(b0);

struct cbPass
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
ConstantBuffer<cbPass> _cbPass : register(b1);

struct cbMaterial
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
};
ConstantBuffer<cbMaterial> _cbMaterial : register(b2);

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
    float4 posW = mul(float4(i.pos, 1.0f), _cbPerObject.Model);
    o.posW = posW.xyz;
    o.normalW = mul(i.normal, (float3x3) _cbPerObject.Model);
    o.pos = mul(posW, _cbPass.VP);
    float4 uv = mul(float4(i.uv, 0.0f, 1.0f), _cbPerObject.TexTransform);
    o.uv = mul(uv, _cbMaterial.MatTransform).xy;

    return o;
}

float4 frag(vOut i) : SV_Target
{
    float4 diffuseAlbedo = _diffuseMap.Sample(_samAnisotropicWrap, i.uv) * _cbMaterial.DiffuseAlbedo;
#ifdef ALPHA_TEST
    clip(diffuseAlbedo.a - 0.1);
#endif
    return 0.2f;
}