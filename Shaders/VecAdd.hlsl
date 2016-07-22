struct Data
{
    float3 v1;
    float2 v2;
};

StructuredBuffer<Data> InputA : register(t0);
StructuredBuffer<Data> InputB : register(t1);
RWStructuredBuffer<Data> Output : register(u0);

[numthreads(32, 1, 1)]
void compAdd(uint3 DTid : SV_DispatchThreadID)
{
    Output[DTid.x].v1 = InputA[DTid.x].v1 + InputB[DTid.x].v1;
    Output[DTid.x].v2 = InputA[DTid.x].v2 + InputB[DTid.x].v2;
}