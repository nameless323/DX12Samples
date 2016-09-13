// bilateral edge preserving blur

cbuffer cbSsao : register(b0)
{
    float4x4 Proj;
    float4x4 InvProj;
    float4x4 OffsetVectors[14];

    float4 BlurWeights[3];

    float2 InvRenderTargetSize;

    float OcclusionRadius;
    float OcclusionFadeStart;
    float OcclusionFadeEnd;
    float SurfaceEpsilon;
};

cbuffer cbRootConstants : register(b1)
{
    bool HorizBlur;
};

Texture2D NormalMap : register(t0);
Texture2D DepthMap : register(t1);
Texture2D InputMap : register(t2);

SamplerState SamPointClamp : register(s0);
SamplerState SamLinearClamp : register(s1);
SamplerState SamDepthMap : register(s2);
SamplerState SamLinearWrap : register(s3);

static const int BlurRadius = 5;
static const float2 TexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

struct vOut
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

vOut vert(uint vid : SV_VertexID)
{
    vOut o;
    o.uv = TexCoords[vid];
    o.pos = float4(2.0f * o.uv.x - 1.0f, 1.0f - 2.0f * o.uv.y, 0.0f, 1.0f);

    return o;
}

float NdcDepthToViewDepth(float zNdc)
{
    // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
    return Proj[3][2] / (zNdc - Proj[2][2]);
}

float4 frag(vOut fIn) : SV_Target
{
    float blurWeights[12] =
    {
        BlurWeights[0].x, BlurWeights[0].y, BlurWeights[0].z, BlurWeights[0].w,
        BlurWeights[1].x, BlurWeights[1].y, BlurWeights[1].z, BlurWeights[1].w,
        BlurWeights[2].x, BlurWeights[2].y, BlurWeights[2].z, BlurWeights[2].w,
    };
    float2 texOffset;
    if (HorizBlur)
    {
        texOffset = float2(InvRenderTargetSize.x, 0.0f);
    }
    else
    {
        texOffset = float2(0.0f, InvRenderTargetSize.y);
    }

    float4 color = blurWeights[BlurRadius] * InputMap.SampleLevel(SamPointClamp, fIn.uv, 0.0);
    float totalWeight = blurWeights[BlurRadius];

    float3 centerNormal = NormalMap.SampleLevel(SamPointClamp, fIn.uv, 0.0f).xyz;
    float centerDepth = NdcDepthToViewDepth(DepthMap.SampleLevel(SamDepthMap, fIn.uv, 0.0f).r);

    for (float i = -BlurRadius; i <= BlurRadius; i++)
    {
        if (i == 0)
            continue;

        float2 tex = fIn.uv + i * texOffset;

        float3 neighborNormal = NormalMap.SampleLevel(SamPointClamp, tex, 0.0).xyz;
        float neighborDepth = NdcDepthToViewDepth(DepthMap.SampleLevel(SamDepthMap, tex, 0.0).r);

        if (dot(neighborNormal, centerNormal >= 0.8) && abs(neighborDepth - centerDepth) <= 0.2)
        {
            float weight = blurWeights[i + BlurRadius];
            color += weight * InputMap.SampleLevel(SamPointClamp, tex, 0.0);
            totalWeight += weight;
        }
    }
    return color / totalWeight;
}