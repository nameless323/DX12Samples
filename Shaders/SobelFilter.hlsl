Texture2D Input : register(t0);
RWTexture2D<float4> Output : register(u0);

float CalcLuminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

[numthreads(16, 16, 1)]
void SobelCS(int3 dispatchThreadID : SV_DispatchThreadID)
{
    float4 c[3][3];
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            int2 xy = dispatchThreadID.xy + int2(-1 + j, -1 + i);
            c[i][j] = Input[xy];
        }
    }
    float4 Gx = -1.0f * c[0][0] - 2.0f * c[1][0] - 1.0f * c[2][0] + 1.0f * c[0][2] + 2.0f * c[1][2] + 1.0f * c[2][2];
    float4 Gy = -1.0f * c[2][0] - 2.0f * c[2][1] - 1.0f * c[2][1] + 1.0f * c[0][0] + 2.0f * c[0][1] + 1.0f * c[0][2];
    float4 mag = sqrt(Gx * Gx + Gy * Gy);
    mag = 1.0f - saturate(CalcLuminance(mag.rgb));
    Output[dispatchThreadID.xy] = mag;
}