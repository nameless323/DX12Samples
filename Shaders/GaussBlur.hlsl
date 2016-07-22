cbuffer cbSettings : register(b0)
{
    int BlurRadius;

    float w0;
    float w1;
    float w2;
    float w3;
    float w4;
    float w5;
    float w6;
    float w7;
    float w8;
    float w9;
    float w10;
};

static const int _maxBlurRadius = 5;

Texture2D _input : register(t0);
RWTexture2D<float4> _output : register(u0);

#define N 256
#define CacheSize (N + 2 * _maxBlurRadius)
groupshared float4 _cache[CacheSize];

[numthreads(N, 1, 1)]
void HorizBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
    float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };

    if (groupThreadID.x < BlurRadius)
    {
        int x = max(dispatchThreadID.x - BlurRadius, 0);
        _cache[groupThreadID.x] = _input[int2(x, dispatchThreadID.y)];
    }
    if (groupThreadID.x >= N - BlurRadius)
    {
        int x = min(dispatchThreadID.x + BlurRadius, _input.Length.x - 1);
        _cache[groupThreadID.x + 2 * BlurRadius] = _input[int2(x, dispatchThreadID.y)];
    }

    _cache[groupThreadID.x + BlurRadius] = _input[min(dispatchThreadID.xy, _input.Length.xy - 1)];

    GroupMemoryBarrierWithGroupSync();

    float4 blurColor = float4(0, 0, 0, 0);

    for (int i = -BlurRadius; i <= BlurRadius; i++)
    {
        int k = groupThreadID.x + BlurRadius + i;
        blurColor += weights[i + BlurRadius] * _cache[k];
    }
    _output[dispatchThreadID.xy] = blurColor;
}

[numthreads(1, N, 1)]
void VertBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
    float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };

    if (groupThreadID.y < BlurRadius)
    {
        int y = max(dispatchThreadID.y - BlurRadius, 0);
        _cache[groupThreadID.y] = _input[int2(dispatchThreadID.x, y)];
    }
    if (groupThreadID.y >= N - BlurRadius)
    {
        int y = min(dispatchThreadID.y + BlurRadius, _input.Length.y - 1);
        _cache[groupThreadID.y + 2 * BlurRadius] = _input[int2(dispatchThreadID.x, y)];
    }

    _cache[groupThreadID.y + BlurRadius] = _input[min(dispatchThreadID.xy, _input.Length.xy - 1)];

    GroupMemoryBarrierWithGroupSync();

    float4 blurColor = float4(0, 0, 0, 0);

    for (int i = -BlurRadius; i <= BlurRadius; i++)
    {
        int k = groupThreadID.y + BlurRadius + i;
        blurColor += weights[i + BlurRadius] * _cache[k];
    }
    _output[dispatchThreadID.xy] = blurColor;
}