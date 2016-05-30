cbuffer buf : register(b0)
{
    float4x4 MVP;
};

void vert(float3 pos : POSITION, float4 col :COLOR0, out float4 opos : SV_Position, out float4 oCol : COLOR0)
{
    opos = mul(float4(pos, 1.0), MVP);
    oCol = col;
}

float4 frag(float4 pos : SV_Position, float4 col : COLOR0) : SV_Target
{
    return col;
}