cbuffer cbPerObject : register(b0)
{
    float4x4 MVP;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut vert(VertexIn vin)
{
    VertexOut vout;
    vout.PosH = mul(float4(vin.PosL, 1.0f), MVP);

    vout.Color = vin.Color;

    return vout;
}

float4 frag(VertexOut pin) : SV_Target
{
    return pin.Color;
}