cbuffer buf : register(b0)
{
    float4x4 MVP;
};

cbuffer buf2 : register(b1)
{
    float TimeCB;
};

void vert(float3 pos : POSITION, float4 col :COLOR0, out float4 opos : SV_Position, out float4 oCol : COLOR0)
{
    //float t = 1;
    float t = TimeCB;
    pos.xy += 0.5 * sin(pos.x) * sin(3.0 * t);
    pos.z *= 0.6 + 0.4 * sin(2.0 * t);
    opos = mul(float4(pos, 1.0), MVP);
    oCol = col;
}

float4 frag(float4 pos : SV_Position, float4 col : COLOR0) : SV_Target
{
    return col;
}