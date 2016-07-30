cbuffer cbUpdateSettings
{
    float WaveConstant0;
    float WaveConstant1;
    float WaveConstant2;

    float DisturbMag;
    int2 DisturbIndex;
};

RWTexture2D<float> _prevSolInput : register(u0);
RWTexture2D<float> _currSolInput : register(u1);
RWTexture2D<float> _output : register(u2);

[numthreads(16, 16, 1)]
void UpdateWavesCS (int3 dispatchThreadID : SV_DispatchThreadID)
{
    int x = dispatchThreadID.x;
    int y = dispatchThreadID.y;

    _output[int2(x, y)] =
    WaveConstant0 * _prevSolInput[int2(x, y)].r +
    WaveConstant1 * _currSolInput[int2(x, y)].r +
    WaveConstant2 * (
        _currSolInput[int2(x, y + 1)].r +
        _currSolInput[int2(x, y - 1)].r +
        _currSolInput[int2(x + 1, y)].r +
        _currSolInput[int2(x - 1, y)].r);

}

[numthreads(1, 1, 1)]
void DisturbWavesCS (int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
    int x = DisturbIndex.x;
    int y = DisturbIndex.y;

    float halfMag = 0.5f * DisturbMag;

    _output[int2(x, y)] += DisturbMag;
    _output[int2(x + 1, y)] += halfMag;
    _output[int2(x - 1, y)] += halfMag;
    _output[int2(x, y + 1)] += halfMag;
    _output[int2(x, y - 1)] += halfMag;
}