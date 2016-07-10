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

Texture2DArray _treeMapArray : register(t0);

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
    float2 Size : SIZE;
};

struct vOut
{
    float3 center : POSITION;
    float2 size : SIZE;
};

struct gOut
{
    float4 pos : SV_Position;
    float3 posW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
    uint PrimID : SV_PrimitiveID;
};

vOut vert(vIn i)
{
    vOut o = (vOut) 0.0;
    o.center = i.pos;
    o.size = i.Size;

    return o;
}

[maxvertexcount(4)]
gOut geom(point vOut i[1], uint primID : SV_PrimitiveID, inout TriangleStream<gOut> triStream)
{
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float3 look = EyePosW - i[0].center;
    look.y = 0.0f;
    look = normalize(look);
    float3 right = cross(up, look);

    float halfWidth = 0.5f * i[0].size.x;
    float halfHeight = 0.5f * i[0].size.y;

    float4 v[4];

    v[0] = float4(i[0].center + halfWidth * right - halfHeight * up, 1.0f);
    v[1] = float4(i[0].center + halfWidth * right + halfHeight * up, 1.0f);
    v[2] = float4(i[0].center - halfWidth * right - halfHeight * up, 1.0f);
    v[3] = float4(i[0].center - halfWidth * right + halfHeight * up, 1.0f);

    float2 texC[4] =
    {
        float2(0.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 1.0f),
		float2(1.0f, 0.0f)
    };
	
    gOut gout;
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        gout.pos = mul(v[i], VP);
        gout.posW = v[i].xyz;
        gout.normalW = look;
        gout.uv = texC[i];

        triStream.Append(gout);
    }

}

float4 frag(gOut i) : SV_Target
{
    float3 uvw = float3(i.uv, i.PrimID % 3);
    float4 diffuseAlbedo = _treeMapArray.Sample(_samAnisotropicWrap, uvw) * DiffuseAlbedo;

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