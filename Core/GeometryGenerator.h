//
// Helper class for different procedural generated meshes.
//

#pragma once

#include <cstdint>
#include <DirectXMath.h>
#include <vector>

class GeometryGenerator
{
public:
    using uint16 = uint16_t;
    using uint32 = uint32_t;
    /**
     * \brief Struct which describes single vertex.
     */
    struct Vertex
    {
        Vertex() {}
        Vertex(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT3& tangent, const DirectX::XMFLOAT2& uv) : Position(pos), Normal(normal), Tangent(tangent), TexCoord(uv)
        {}
        Vertex(
            float px, float py, float pz,
            float nx, float ny, float nz,
            float tx, float ty, float tz,
            float u, float v
            ) : Position(px, py, pz), Normal(nx, ny, nz), Tangent(tx, ty, tz), TexCoord(u, v)
        {}

        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT3 Tangent;
        DirectX::XMFLOAT2 TexCoord;
    };
    
     /**
     * \brief Struct which describes mesh data (indices and vertices).
     */
    struct MeshData
    {
        std::vector<Vertex> Vertices;
        std::vector<uint32> Indices32;

        std::vector<uint16>& GetIndices16()
        {
            if (_indices16.empty())
            {
                _indices16.resize(Indices32.size());
                for (size_t i = 0; i < Indices32.size(); i++)
                {
                    _indices16[i] = static_cast<uint16>(Indices32[i]);
                }
            }
            return _indices16;
        }
    private:
        std::vector<uint16> _indices16;
    };
     /**
     * \brief Struct which describes single vertex. Creates a box centered at the origin with the given dimensions, where each
     * face has m rows and n columns of vertices.
     */
    MeshData CreateBox(float width, float height, float depth, uint32 numSubdivisions);
    /**
     * \brief Struct which describes single vertex. Creates a sphere centered at the origin with the given radius.  The
     * slices and stacks parameters control the degree of tessellation.
     */
    MeshData CreateSphere(float radius, uint32 sliceCount, uint32 stackCount);
    /**
     * \brief Creates a geosphere centered at the origin with the given radius.  The
     * depth controls the level of tessellation.
     */
    MeshData CreateGeosphere(float radius, uint32 numSubdivisions);
    /**
     * \brief Creates a cylinder parallel to the y-axis, and centered about the origin.  
     * The bottom and top radius can vary to form various cone shapes rather than true
     * cylinders.  The slices and stacks parameters control the degree of tessellation.
     */
    MeshData CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount);
    /**
     * \brief Creates an mxn grid in the xz-plane with m rows and n columns, centered
     * at the origin with the specified width and depth.
     */
    MeshData CreateGrid(float width, float depth, uint32 m, uint32 n);
    /**
     * \brief Creates a quad aligned with the screen.  This is useful for postprocessing and screen effects.
     */
    MeshData CreateQuad(float x, float y, float w, float h, float depth);

private:
    void Subdivide(MeshData& meshData);
    Vertex MidPoint(const Vertex& v0, const Vertex& v1);
    void BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData);
    void BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData);
};
