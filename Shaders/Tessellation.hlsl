#include "LightingUtil.hlsl"

Texture2D _diffuseMap : register(t0);
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
    float4 pos : POSITION;
};

struct vOut
{
    float4 pos : POSITION;
};

vOut vert(vIn i)
{
    vOut o;
    o.pos = i.pos;
    return o;
}

struct PatchTess
{
    float EdgeTess[4] : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

PatchTess ConstantHull (InputPatch<vOut, 4> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;
    float3 centerL = 0.25f * (patch[0].pos + patch[1].pos + patch[2].pos + patch[3].pos);
    float3 centerW = mul(float4(centerL, 1.0f), Model).xyz;

    float d = distance(centerW, EyePosW);

    const float d0 = 20.0f;
    const float d1 = 100.0f;
    float tess = 64.0f * saturate((d1 - d) / (d1 - d0));

    pt.EdgeTess[0] = tess;
    pt.EdgeTess[1] = tess;
    pt.EdgeTess[2] = tess;
    pt.EdgeTess[3] = tess;

    pt.InsideTess[0] = tess;
    pt.InsideTess[1] = tess;

    return pt;
}

struct HullOut
{
    float3 pos : POSITION;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ConstantHull")]
[maxtessfactor(64.0f)]
HullOut hull(InputPatch<vOut, 4> p, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
    HullOut hout;
    hout.pos = p[i].pos;
    return hout;
}

struct DomainOut
{
    float pos : SV_Position;
};

[domain("quad")]
DomainOut domain(PatchTess passTess, float2 uv : SV_DomainLocation, const OutputPatch<HullOut, 4> quad)
{
    DomainOut dout;
    float3 v1 = lerp(quad[0].pos, quad[1].pos, uv.x);
    float3 v2 = lerp(quad[2].pos, quad[3].pos, uv.x);
    float3 p = lerp(v1, v2, uv.y);

    p.y = 0.3f * (p.z * sin(p.x) + p.x * cos(p.z));

    float4 posw = mul(float4(p, 1.0f), Model);
    dout.pos = mul(posw, VP);

    return dout;
}

float4 frag(DomainOut pin) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}