StructuredBuffer<float3> Input : register(t0);
RWStructuredBuffer<float> Output : register(u0);

[numthreads(64, 1, 1)]
void main (uint3 DTid : SV_DispatchThreadID)
{
    float3 input = Input[DTid.x];
    Output[DTid.x] = length(input);
}