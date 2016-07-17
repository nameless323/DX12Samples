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

struct hOut
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct dOut
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

struct ConstHullOutput
{
    float EdgeTess[3] : SV_TessFactor;
    float InsideTess : SV_InsideTessFactor;
};

ConstHullOutput ConstantHull(InputPatch<vOut, 3> ip, uint patchId : SV_PrimitiveID)
{
    float3 center = (ip[0].pos + ip[1].pos + ip[2].pos)/3.0;
    float dist = distance(EyePosW, mul(float4(center, 1.0), Model).xyz);

    const float d0 = 5;
    const float d1 = 10;
    const float tessMin = 1.0;
    const float tessMax = 16.0;
    float factor = (dist - d0) / (d1 - d0);
    float tessFactor = lerp(tessMax, tessMin, saturate(factor));

    ConstHullOutput o;
    o.EdgeTess[0] = tessFactor;
    o.EdgeTess[1] = tessFactor;
    o.EdgeTess[2] = tessFactor;
    o.InsideTess = tessFactor;

    return o;
}

[domain("tri")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHull")]
hOut hull(InputPatch<vOut, 3> ip, uint inId : SV_OutputControlPointID , uint patchId : SV_PrimitiveID)
{
    hOut o;
    o.normal = ip[inId].normal;
    o.pos = ip[inId].pos;
    o.uv = ip[inId].uv;
    return o;
}


[domain("tri")]
dOut dom(ConstHullOutput i, float3 uv : SV_DomainLocation, const OutputPatch<hOut, 3> trig)
{
    dOut o;
    float3 pos = trig[0].pos * uv.x + trig[1].pos * uv.y + trig[2].pos * uv.z;
    float2 tc = trig[0].uv * uv.x + trig[1].uv * uv.y + trig[2].uv * uv.z;
    pos = normalize(pos);
    float4 wPos = mul(float4(pos, 1.0), Model);
    o.posW = wPos.xyz;
    o.normalW = mul(pos, (float3x3)Model);
    o.uv = tc;
    o.pos = mul(wPos, VP); 
    
    return o;
}


float4 frag(dOut i) : SV_Target
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