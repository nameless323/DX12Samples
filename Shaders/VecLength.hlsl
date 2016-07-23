StructuredBuffer<float3> Input;
StructuredBuffer<float> Output;

[numthreads(64, 1, 1)]
void main (uint3 DTid : SV_DispatchThreadID)
{
    float3 input = Input[DTid.x];
    Output[DTid.x] = sqrt(dot(input, input));
}