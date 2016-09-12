cbuffer cbSSAO : register(b0)
{
    float4x4 Proj;
    float4x4 InvProj;
    float4x4 ProjTex;
    float4 OffsetVectors[14];

    float4 BlurWeights[3];
    float2 InvRenderTargetSize;

    float OcclusionRadius;
    float OcclusionFadeStart;
    float OcclusionFadeEnd;
    float SurfaceEpsilon;
};

cbuffer cbRootConstants : register(b1)
{
    bool HorizontalBlur;
};

Texture2D NormalMap : register(t0);
Texture2D DepthMap : register(t1);
Texture2D RandomVecMap : register(t2);

SamplerState SamPointClamp : register(s0);
SamplerState SamLinearClamp : register(s0);
SamplerState SamDepthMap : register(s0);
SamplerState SamLinearWrap : register(s0);

static const int SampleCount = 14;

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
    float4 Pos : SV_Position;
    float3 PosV : POSITION;
    float2 uv : TEXCOORD;
};

vOut vert(uint vid : SV_VertexID)
{
    vOut vout;
    vout.uv = TexCoords[vid];
    vout.Pos = float4(2.0f * vout.uv.x - 1.0f, 1.0f - 2.0f * vout.uv.y, 0.0f, 1.0f);

    float4 ph = mul(vout.Pos, InvProj);
    vout.PosV = ph.xyz / ph.w;

    return vout;
}

float OcclusionFunction(float distZ)
{
    float occlusion = 0.0f;
    if (distZ > SurfaceEpsilon)
    {
        float fadeLength = OcclusionFadeEnd - OcclusionFadeStart;
        occlusion = saturate((OcclusionFadeEnd - distZ) / fadeLength);
    }
    return occlusion;
}

float NdcDepthToViewDepth(float zNdc)
{
    // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
    float viewZ = Proj[3][2] / (zNdc - Proj[2][2]);
    return viewZ;
}

float4 frag(vOut i) : SV_Target
{
    float3 n = normalize(NormalMap.SampleLevel(SamPointClamp, i.uv, 0.0f).xyz);
    float pz = DepthMap.SampleLevel(SamDepthMap, i.uv, 0.0f).r;
    pz = NdcDepthToViewDepth(pz);

    float3 p = (pz / i.PosV.z) * i.PosV;
    float3 randVec = 2.0f * RandomVecMap.SampleLevel(SamLinearWrap, 4.0f * i.uv, 0.0f);

    float occlusionSum = 0.0f;

    for (int i = 0; i < SampleCount; i++)
    {
        float3 offset = reflect(OffsetVectors[i].xyz, randVec);

        float flip = sign(dot(offset, n));
        float3 q = p + flip * OcclusionRadius * offset;

        float4 projQ = mul(float4(q, 1.0f), ProjTex);
        projQ /= projQ.w;

        float rz = DepthMap.SampleLevel(SamDepthMap, projQ.xy, 0.0f).r;
        rz = NdcDepthToViewDepth(rz);

        float3 r = (rz / q.z) * q;

        float distZ = p.z - r.z;
        float dp = max(dot(n, normalize(r - p)), 0.0f);
        float occlusion = dp * OcclusionFunction(distZ);
        occlusionSum += occlusion;
    }
    occlusionSum /= SampleCount;
    float access = 1.0f - occlusionSum;

    return saturate(pow(access, 6.0f));
}