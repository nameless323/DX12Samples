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
    float3 pos : POSITION;
};

struct vOut
{
    float3 pos : POSITION;
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

PatchTess ConstantHull(InputPatch<vOut, 16> patch, uint patchID : SV_PrimitiveID)
{
    PatchTess pt;

    pt.EdgeTess[0] = 25;
    pt.EdgeTess[1] = 25;
    pt.EdgeTess[2] = 25;
    pt.EdgeTess[3] = 25;

    pt.InsideTess[0] = 25;
    pt.InsideTess[1] = 25;

    return pt;
}

struct HullOut
{
    float3 pos : POSITION;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(16)]
[patchconstantfunc("ConstantHull")]
[maxtessfactor(64.0f)]
HullOut hull(InputPatch<vOut, 16> p, uint i : SV_OutputControlPointID, uint patchId : SV_PrimitiveID)
{
    HullOut hout;
    hout.pos = p[i].pos;
    return hout;
}

struct DomainOut
{
    float4 pos : SV_Position;
};

float4 BernsteinBasis(float t)
{
    float invT = 1.0f - t;

    return float4(invT * invT * invT,
                   3.0f * t * invT * invT,
                   3.0f * t * t * invT,
                   t * t * t);
}

float3 CubicBezierSum(const OutputPatch<HullOut, 16> bezpatch, float4 basisU, float4 basisV)
{
    float3 sum = float3(0.0f, 0.0f, 0.0f);
    sum = basisV.x * (basisU.x * bezpatch[0].pos + basisU.y * bezpatch[1].pos + basisU.z * bezpatch[2].pos + basisU.w * bezpatch[3].pos);
    sum += basisV.y * (basisU.x * bezpatch[4].pos + basisU.y * bezpatch[5].pos + basisU.z * bezpatch[6].pos + basisU.w * bezpatch[7].pos);
    sum += basisV.z * (basisU.x * bezpatch[8].pos + basisU.y * bezpatch[9].pos + basisU.z * bezpatch[10].pos + basisU.w * bezpatch[11].pos);
    sum += basisV.w * (basisU.x * bezpatch[12].pos + basisU.y * bezpatch[13].pos + basisU.z * bezpatch[14].pos + basisU.w * bezpatch[15].pos);

    return sum;
}

float4 dBernsteinBasis(float t)
{
    float invT = 1.0f - t;

    return float4(-3 * invT * invT,
                   3 * invT * invT - 6 * t * invT,
                   6 * t * invT - 3 * t * t,
                   3 * t * t);
}

[domain("quad")]
DomainOut domain(PatchTess patchTess, float2 uv : SV_DomainLocation, const OutputPatch<HullOut, 16> bezPatch)
{
    DomainOut dout;
    float4 basisU = BernsteinBasis(uv.x);
    float4 basisV = BernsteinBasis(uv.y);

    float3 p = CubicBezierSum(bezPatch, basisU, basisV);
    float4 posw = mul(float4(p, 1.0), Model);
    dout.pos = mul(posw, VP);

    return dout;
}

float4 frag(DomainOut i) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0);
}
