//
// Describes water geometry on GPU.
//

#pragma once

#include "../../Core/D3DUtil.h"
#include "../../Core/GameTimer.h"

namespace DX12Samples
{
class GpuWaves
{
public:
    GpuWaves(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, int m, int n, float dx, float dt, float speed, float damping);
    GpuWaves(const GpuWaves& rhs) = delete;
    GpuWaves& operator= (const GpuWaves& rhs) = delete;
    ~GpuWaves() = default;
    /**
     * \brief Get waves mesh row count. 
     */
    UINT RowCount() const;
    /**
     * \brief Get waves mesh column count.
     */
    UINT ColumnCount() const;
    /**
     * \brief Get waves mesh vertex count.
     */
    UINT VertexCount() const;
    /**
     * \brief Get waves mesh triangle count.
     */
    UINT TriangleCount() const;
    /**
     * \brief Get waves mesh spatial width.
     */
    float Width() const;
    /**
     * \brief Get waves mesh spatial height.
     */
    float Height() const;
    /**
     * \brief Get spatial step between mesh triangles.
     */
    float SpatialStep() const;
    /**
     * \brief Get handle to waves displacement map.
     */
    CD3DX12_GPU_DESCRIPTOR_HANDLE DisplacementMap() const;
    /**
     * \brief Get nesessary descriptor count for water simulation. 
     */
    UINT DescriptorCount() const;

    /**
     * \brief Build resoruces.
     */
    void BuildResources(ID3D12GraphicsCommandList* cmdList);
    /**
     * \brief Build descriptors.
     * \param hCpuDescriptor CPU descriptor in prebuilt heap.
     * \param hGpuDescriptor GPU descriptor in prebuilt heap.
     * \param descriptorSize CBV SRV UAV descriptor size to offset in heap.
     */
    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize);
    /**
     * \brief Update wavws geometry. 
     */
    void Update(const GameTimer& timer, ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSig, ID3D12PipelineState* pso);
    /**
     * \brief Disturb waves at index i, j.
     */
    void Disturb(ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSig, ID3D12PipelineState* pso, UINT i, UINT j, float magnitude);

private:
    UINT _numRows;
    UINT _numCols;

    UINT _vertexCount;
    UINT _triangleCount;

    float _K[3];
    float _timeStep;
    float _spatialStep;

    ID3D12Device* _device = nullptr;

    CD3DX12_GPU_DESCRIPTOR_HANDLE _prevSolSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _currSolSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _nextSolSrv;

    CD3DX12_GPU_DESCRIPTOR_HANDLE _prevSolUav;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _currSolUav;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _nextSolUav;

    Microsoft::WRL::ComPtr<ID3D12Resource> _prevSol = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _currSol = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _nextSol = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> _prevUploadBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _currUploadBuffer = nullptr;
};
}