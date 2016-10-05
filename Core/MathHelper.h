//
// Some helper functions for mathematics.
//

#pragma once

#include <Windows.h>
#include <DirectXMath.h>

class MathHelper
{
public:
    /**
     * \brief Get random float in [0, 1).
     */
    static float RandF()
    {
        return (float)(rand()) / (float)RAND_MAX;
    }

    /**
     * \brief Get random float in [a, b).
     */
    static float RandF(float a, float b)
    {
        return a + (b - a)*RandF();
    }
    /**
     * \brief Get random int in [a, b].
     */
    static int Rand(int a, int b)
    {
        return a + rand() % ((b - a) + 1);
    }
    /**
     * \brief Get mininmum from a, b.
     */
    template<typename T>
    static T Min(const T& a, const T& b)
    {
        return a < b ? a : b;
    }
    /**
     * \brief Get maximum from a, b.
     */
    template<typename T>
    static T Max(const T& a, const T& b)
    {
        return a > b ? a : b;
    }
    /**
     * \brief Get linear interpolation from a to b with weight t.
     */
    template<typename T>
    static T Lerp(const T& a, const T& b, float t)
    {
        return a + (b - a) * t;
    }
    /**
     * \brief Clamps x to [low, high].
     */
    template<typename T>
    static T Clamp(const T& x, const T& low, const T& high)
    {
        return x < low ? low : (x > high ? high : x);
    }
    /**
     * \brief Clamps x to [0, 1].
     */
    template<typename T>
    static T Clamp01(const T& x)
    {
        return x < 0 ? 0 : (x > 1 ? 1 : x);
    }
    /**
     * \brief Returns polar angle of the point in [0, 2*PI].
     */
    static float AngleFromXY(float x, float y);
    /**
     * \brief Converts spherical coordinates to cartesian.
     */
    static DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi)
    {
        return DirectX::XMVectorSet(radius * sinf(phi) * cosf(theta),
            radius*cosf(phi),
            radius*sinf(phi)*sinf(theta),
            1.0f);
    }
    /**
     * \brief Find inverse transpose from transform M. WARNING: use with caution, wery expencive operation.
     */
    static DirectX::XMMATRIX InverseTranspose(DirectX::CXMMATRIX M)
    {
        DirectX::XMMATRIX A = M;
        A.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

        DirectX::XMVECTOR det = XMMatrixDeterminant(A);
        return XMMatrixTranspose(XMMatrixInverse(&det, A));
    }
    /**
     * \brief Get identity matrix.
     */
    static DirectX::XMFLOAT4X4 Identity4x4()
    {
        static DirectX::XMFLOAT4X4 I
            (
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 1.0f
            );
        return I;
    }
    /**
     * \brief Get random unit vector.
     */
    static DirectX::XMVECTOR RandUnitVec3();
    /**
     * \brief Get random unit vector on hemisphere.
     */
    static DirectX::XMVECTOR RandHemisphereUnitVec3(DirectX::XMVECTOR n);

    static const float Infinity;
    static const float Pi;
};
