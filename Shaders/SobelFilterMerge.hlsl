Texture2D MainTex : register(t0);
Texture2D SobelTex : register(t1);

SamplerState _samPointWrap : register(s0);
SamplerState _samPointClamp : register(s1);
SamplerState _samLinearWrap : register(s2);
SamplerState _samLinearClamp : register(s3);
SamplerState _samAnisotropicWrap : register(s4);
SamplerState _samAnisotropicClamp : register(s5);

static const float2 uv[6] =
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
    o.uv = uv[vid];
    o.pos = float4(2.0 * o.uv.x - 1.0,  1.0 - 2.0 * o.uv.y, 0.0, 1.0);

    return o;
}

float4 frag(vOut i) : SV_Target
{
    float4 c = MainTex.SampleLevel(_samPointClamp, i.uv, 0.0f);
    float4 e = SobelTex.SampleLevel(_samPointClamp, i.uv, 0.0f);
    return c * e;
}