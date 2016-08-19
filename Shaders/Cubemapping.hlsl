#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "CommonInclude.hlsl"

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
    float4 posW = mul(float4(i.pos, 1.0f), Model);
    o.posW = posW.xyz;
    o.normalW = mul(i.normal, (float3x3) Model);
    o.pos = mul(posW, VP);
    float4 uv = mul(float4(i.uv, 0.0f, 1.0f), TexTransform);

    MaterialData matData = _materialData[MaterialIndex];
    o.uv = mul(uv, matData.MatTransform).xy;

    return o;
}

float4 frag(vOut i) : SV_Target
{
    MaterialData matData = _materialData[MaterialIndex];
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    float3 fresnelR0 = matData.FresnelR0;
    float roughness = matData.Roughness;
    uint diffuseTexIndex = matData.DiffuseMapIndex;

    diffuseAlbedo *= _diffuseMap[diffuseTexIndex].Sample(_samAnisotropicWrap, i.uv);

    i.normalW = normalize(i.normalW);
    float3 toEyeW = normalize(EyePosW - i.posW);

    float4 ambient = AmbientLight * diffuseAlbedo;

    const float shininess = 1.0f - roughness;
    Material mat = { diffuseAlbedo, fresnelR0, shininess };
    float3 shadowFactor = 1.0;
    float4 directLight = ComputeLighting(Lights, mat, i.posW, i.normalW, toEyeW, shadowFactor);
    float4 litColor = ambient + directLight;

    float3 r = reflect(-toEyeW, i.normalW);
    float4 reflectionColor = _cubeMap.Sample(_samAnisotropicWrap, r);
    float3 fresnelFactor = SchlickFresnel(fresnelR0, i.normalW, r);
    litColor.rgb += shininess * fresnelFactor * reflectionColor.rbg;
    litColor.a = diffuseAlbedo.a;

    return litColor;
}