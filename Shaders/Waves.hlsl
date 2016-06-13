cbuffer cbPerObject : register(b0)
{
    float4x4 Model;
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
};

struct vIn
{
    float3 pos : POSITION;
    float4 color : COLOR0;
};

struct vOut
{
    float4 pos : SV_Position;
    float4 color : COLOR0;
};

vOut vert(vIn i)
{
    vOut o;
    o.pos = mul(float4(i.pos, 1.0), Model);
    o.pos = mul(o.pos, VP);
    o.color = i.color;
	return o;
}

float4 frag(vOut i) : SV_Target
{
    return i.color;
}