//
// Describes waves on CPU.

#pragma once

#include <vector>
#include <DirectXMath.h>

namespace DX12Samples
{
class Waves
{
public:
    Waves(int m, int n, float dx, float dt, float speed, float damping);
    Waves(const Waves& rhs) = delete;
    Waves& operator=(const Waves& rhs) = delete;
    ~Waves();
    /**
     * \brief Get waves vertices row count.
     */
    int RowCount() const;
    /**
     * \brief Get waves vertices column count.
     */
    int ColumnCount() const;
    /**
     * \brief Get waves mesh vertex count.
     */
    int VertexCount() const;
    /**
     * \brief Get waves mesh triangle count.
     */
    int TriangleCount() const;
    /**
     * \brief Get waves mesh spatial width.
     */
    float Width() const;
    /**
     * \brief Get spatial count spatial height.
     */
    float Depth() const;
    /**
     * \brief Gey Ith vertex position.
     */
    const DirectX::XMFLOAT3& Position(int i) const
    {
        return _currSolution[i];
    }
    /**
    * \brief Gey Ith vertex normal.
    */
    const DirectX::XMFLOAT3& Normal(int i) const
    {
        return _normals[i];
    }
    /**
    * \brief Gey Ith vertex tangent.
    */
    const DirectX::XMFLOAT3& TangentX(int i) const
    {
        return _tangentX[i];
    }
    /**
     * \brief Update waves geometry.
     */
    void Update(float dt);
    /**
     * \brief Disturb wave at i,j index with magnitude.
     */
    void Disturb(int i, int j, float magnitude);

private:
    int _numRows = 0;
    int _numColumns = 0;
    int _vertexCount = 0;
    int _triangleCount = 0;

    float _k1 = 0.0f;
    float _k2 = 0.0f;
    float _k3 = 0.0f;

    float _timeStep = 0.0f;
    float _spatialStep = 0.0f;

    std::vector<DirectX::XMFLOAT3> _prevSolution;
    std::vector<DirectX::XMFLOAT3> _currSolution;
    std::vector<DirectX::XMFLOAT3> _normals;
    std::vector<DirectX::XMFLOAT3> _tangentX;
};
}