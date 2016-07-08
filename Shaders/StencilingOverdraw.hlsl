cbuffer cbIndex : register(b3)
{
    int Index;
};

//ConstantBuffer<cbIndex> _colorIndex : register(b3);


float4 vert(uint vertId : SV_VertexID) : SV_Position
{
    float2 texcoord = float2(vertId & 1, vertId >> 1);
    return float4((texcoord.x - 0.5f) * 2, -(texcoord.y - 0.5f) * 2, 0, 1);
}

float4 frag(float4 i : SV_Position) : SV_Target
{
    const float4 colors[] =
    {
        float4(1, 1, 1, 1),
        float4(0, 0, 0, 1),
        float4(1, 0, 0, 1),
        float4(0, 1, 0, 1),
        float4(0, 0, 1, 1),
        float4(1, 1, 0, 1),
        float4(1, 0, 1, 1),
        float4(0, 1, 1, 1),
        float4(0.4, 0.7, 0.3, 1),
        float4(0.5, 0.5, 0.9, 1),
    };
    return colors[(int)Index];
}