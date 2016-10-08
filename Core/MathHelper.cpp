#include "MathHelper.h"

#include <float.h>
#include <cmath>

namespace DX12Samples
{
using namespace DirectX;

const float MathHelper::Infinity = FLT_MAX;
const float MathHelper::Pi = 3.1415926535f;

float MathHelper::AngleFromXY(float x, float y)
{
    float theta = 0.0f;
    if (x >= 0.0f)
    {
        theta = atan(y / x);
        if (theta < 0.0f)
            theta += 2.0f * Pi;
    }
    else
        theta = atan(y / x) + Pi;
    return theta;
}

XMVECTOR MathHelper::RandUnitVec3()
{
    XMVECTOR One = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);

    while (true)
    {
        XMVECTOR v = XMVectorSet(RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), 0.0f);
        if (XMVector3Greater(XMVector3LengthSq(v), One))
            continue;
        return XMVector3Normalize(v);
    }
}

XMVECTOR MathHelper::RandHemisphereUnitVec3(XMVECTOR n)
{
    XMVECTOR One = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
    XMVECTOR Zero = XMVectorZero();

    while (true)
    {
        XMVECTOR v = XMVectorSet(RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), RandF(-1.0f, 1.0f), 0.0f);
        if (XMVector3Greater(XMVector3LengthSq(v), One))
            continue;
        if (XMVector3Less(XMVector3Dot(n, v), Zero))
            continue;
        return XMVector3Normalize(v);
    }
}
}