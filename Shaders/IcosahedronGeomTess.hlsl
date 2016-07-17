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
Texture2D _fence : register(t1);

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
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct gOut
{
    float4 pos : SV_Position;
    float3 posW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
};

vOut vert(vIn i)
{
    vOut o = (vOut) 0.0;
    o.pos = i.pos;
    o.normal = i.normal;
    o.uv = i.uv;

    return o;
}

[maxvertexcount(8)]
void geom(triangle vOut i[3], inout TriangleStream<gOut> o)
{
    gOut outVerts[6];
    [unroll]
    for (int j = 0; j < 3; j++)
    {
        outVerts[j].pos.xyz = i[j].pos;
        outVerts[j].pos.w = 1.0;
        outVerts[j].normalW = i[j].normal;
        outVerts[j].uv = i[j].uv;
    }

    gOut vert;
    vert.pos.xyz = normalize(lerp(i[0].pos, i[1].pos, 0.5));
    vert.pos.w = 1.0;
    vert.normalW = vert.pos.xyz; // its just a sphere isnt it?
    vert.uv = lerp(i[0].uv, i[1].uv, 0.5);
    outVerts[3] = vert;

    vert.pos.xyz = normalize(lerp(i[1].pos, i[2].pos, 0.5));
    vert.normalW = vert.pos.xyz;
    vert.uv = lerp(i[1].uv, i[2].uv, 0.5);
    outVerts[4] = vert;

    vert.pos.xyz = normalize(lerp(i[0].pos, i[2].pos, 0.5));
    vert.normalW = vert.pos.xyz;
    vert.uv = lerp(i[0].uv, i[2].uv, 0.5);
    outVerts[5] = vert;

    [unroll]
    for (int k = 0; k < 6; k++)
    {
        float4 posw = mul(outVerts[k].pos, Model);
        outVerts[k].posW = posw.xyz;
        outVerts[k].normalW = mul(outVerts[k].normalW, (float3x3) Model);
        outVerts[k].pos = mul(posw, VP);
    }

    o.Append(outVerts[0]);
    o.Append(outVerts[3]);
    o.Append(outVerts[5]);
    o.Append(outVerts[4]);
    o.Append(outVerts[2]);

    o.RestartStrip();
    o.Append(outVerts[3]);
    o.Append(outVerts[1]);
    o.Append(outVerts[4]);
}

float4 frag(gOut i) : SV_Target
{
    //SamplerState st[] = { _samLinearClamp, _samPointClamp, _samAnisotropicClamp };
    float4 diffuseAlbedo = _diffuseMap.Sample(_samAnisotropicClamp, i.uv) * DiffuseAlbedo;
    //float4 fenceColor = _fence.Sample(_samLinearWrap, i.uv) * DiffuseAlbedo;
    //diffuseAlbedo = lerp(diffuseAlbedo, fenceColor, fenceColor.a);

    i.normalW = normalize(i.normalW);
    float3 toEyeW = normalize(EyePosW - i.posW);

    float4 ambient = AmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - Roughness;
    Material mat = { diffuseAlbedo, FresnelR0, shininess };
    float3 shadowFactor = 1.0;
    float4 directLight = ComputeLighting(Lights, mat, i.posW, i.normalW, toEyeW, shadowFactor);
    float4 litColor = ambient + directLight;
    litColor.a = diffuseAlbedo.a;

    return litColor;
}