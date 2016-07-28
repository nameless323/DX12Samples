#pragma once
#include "../../Core/D3DUtil.h"
#include "../../Core/GameTimer.h"

class GpuWaves
{
public:
    GpuWaves(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, int m, int n, float dx, float dt, float speed, float damping);
    GpuWaves(const GpuWaves& rhs) = delete;
    GpuWaves& operator= (const GpuWaves& rhs) = delete;
    ~GpuWaves() = default;

    UINT RowCount() const;
    UINT ColumnCount() const;
    UINT VertexCount() const;
    UINT TriangleCount() const;
    float Width() const;
    float Height() const;
    float SpatialStep() const;

    CD3DX12_GPU_DESCRIPTOR_HANDLE DisplacementMap() const;

    UINT DescriptorCount() const;

    void BuildResources(ID3D12GraphicsCommandList* cmdList);
    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize);
    void Update(const GameTimer& timer, ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSig, ID3D12PipelineState* pso);
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