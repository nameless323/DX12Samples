#include "Waves.h"

#include <ppl.h>

namespace DX12Samples
{
using namespace DirectX;

Waves::Waves(int m, int n, float dx, float dt, float speed, float damping) : _numRows(m), _numColumns(n), _vertexCount(m * n), _triangleCount((m - 1) * (n - 1) * 2), _timeStep(dt), _spatialStep(dx)
{
    float d = damping * dt + 2.0f;
    float e = (speed*speed) * (dt * dt) / (dx*dx);
    _k1 = (damping * dt - 2.0f) / d;
    _k2 = (4.0f - 8.0f * e) / d;
    _k3 = (2.0f * e) / d;

    _prevSolution.resize(m * n);
    _currSolution.resize(m * n);
    _normals.resize(m * n);
    _tangentX.resize(m * n);

    float halfWidth = (n - 1) * dx * 0.5f;
    float halfDepth = (m - 1) * dx * 0.5f;
    for (int i = 0; i < m; i++)
    {
        float z = halfDepth - i * dx;
        for (int j = 0; j < n; j++)
        {
            float x = -halfWidth + j * dx;

            _prevSolution[i * n + j] = XMFLOAT3(x, 0.0f, z);
            _currSolution[i * n + j] = XMFLOAT3(x, 0.0f, z);
            _normals[i * n + j] = XMFLOAT3(0.0f, 1.0f, 0.0f);
            _tangentX[i * n + j] = XMFLOAT3(1.0f, 0.0f, 0.0f);
        }
    }
} 

Waves::~Waves()
{    
}

int Waves::RowCount() const
{
    return _numRows;
}

int Waves::ColumnCount() const
{
    return _numColumns;
}

int Waves::VertexCount() const
{
    return _vertexCount;
}

int Waves::TriangleCount() const
{
    return _triangleCount;
}

float Waves::Width() const
{
    return _numColumns * _spatialStep;
}

float Waves::Depth() const
{
    return _numRows * _spatialStep;
}

void Waves::Update(float dt)
{
    static float t = 0;
    t += dt;

    if (t >= _timeStep)
    {

        concurrency::parallel_for(1, _numRows - 1, [this](int i) // for (int i = 1; i < _numRows - 1; i ++)
        {
            for (int j = 1; j < _numColumns - 1; j++)
            {
                _prevSolution[i * _numColumns + j].y =
                    _k1 * _prevSolution[i * _numColumns + j].y +
                    _k2 * _currSolution[i * _numColumns + j].y +
                    _k3 * (_currSolution[(i + 1) * _numColumns + j].y + _currSolution[(i - 1) * _numColumns + j].y + _currSolution[i * _numColumns + j + 1].y + _currSolution[i * _numColumns + j - 1].y);
            }
        });

        swap(_prevSolution, _currSolution);
        t = 0.0f;

        concurrency::parallel_for(1, _numRows - 1, [this](int i)
        {
            for (int j = 1; j < _numColumns - 1; j++)
            {
                int currIndex = i * _numColumns + j;
                float l = _currSolution[currIndex - 1].y;
                float r = _currSolution[currIndex + 1].y;
                float t = _currSolution[(i - 1) * _numColumns + j].y;
                float b = _currSolution[(i + 1) * _numColumns + j].y;
                _normals[currIndex].x = -r + l;
                _normals[currIndex].y = 2.0f * _spatialStep;
                _normals[currIndex].z = b - t;

                XMVECTOR n = XMVector3Normalize(XMLoadFloat3(&_normals[currIndex]));
                XMStoreFloat3(&_normals[currIndex], n);

                _tangentX[currIndex] = XMFLOAT3(2.0f*_spatialStep, r - l, 0.0f);
                XMVECTOR T = XMVector3Normalize(XMLoadFloat3(&_tangentX[currIndex]));
                XMStoreFloat3(&_tangentX[currIndex], T);
            }
        });
    }
}

void Waves::Disturb(int i, int j, float magnitude)
{
    assert(i > 1 && i < _numRows - 2);
    assert(j > 1 && j < _numColumns - 2);

    float halfMag = 0.5f * magnitude;

    _currSolution[i * _numColumns + j].y += magnitude;
    _currSolution[i * _numColumns + j + 1].y += halfMag;
    _currSolution[i * _numColumns + j - 1].y += halfMag;
    _currSolution[(i + 1) * _numColumns + j].y += halfMag;
    _currSolution[(i - 1) * _numColumns + j].y += halfMag;
}
}