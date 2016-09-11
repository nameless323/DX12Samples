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

float4 frag(vOut i) : SV_Target
{
    
}