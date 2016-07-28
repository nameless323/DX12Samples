#include "GpuWaves.h"
#include <cassert>
#include <vector>

GpuWaves::GpuWaves(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, int m, int n, float dx, float dt, float speed, float damping)
{
    _device = device;
    _numRows = m;
    _numCols = n;

    assert((n * n) % 256 == 0);

    _vertexCount = m * n;
    _triangleCount = (m - 1) * (n - 1) * 2;

    _timeStep = dt;
    _spatialStep = dx;

    float d = damping * dt + 2.0f;
    float e = speed * speed * dt * dt / (dx * dx);
    _K[0] = (damping * dt - 2.0f) / d;
    _K[1] = (4.0f - 8.0f * e) / d;
    _K[2] = (2.0f * e) / d;

    BuildResources(cmdList);
}

UINT GpuWaves::RowCount() const
{
    return _numRows;
}

UINT GpuWaves::ColumnCount() const
{
    return _numCols;
}

UINT GpuWaves::VertexCount() const
{
    return _vertexCount;
}

UINT GpuWaves::TriangleCount() const
{
    return _triangleCount;
}

float GpuWaves::Width() const
{
    return _numRows * _spatialStep;
}

float GpuWaves::Height() const
{
    return _numCols * _spatialStep;
}

float GpuWaves::SpatialStep() const
{
    return _spatialStep;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GpuWaves::DisplacementMap() const
{
    return _currSolSrv;
}

UINT GpuWaves::DescriptorCount() const
{
    return 6;
}

void GpuWaves::BuildResources(ID3D12GraphicsCommandList* cmdList)
{
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = _numCols;
    texDesc.Height = _numRows;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R32_FLOAT;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ThrowIfFailed(_device->CreateCommittedResource
        (
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&_prevSol)
        ));
    ThrowIfFailed(_device->CreateCommittedResource
        (
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&_currSol)
        ));
    ThrowIfFailed(_device->CreateCommittedResource
        (
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&_nextSol)
        ));

    const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(_currSol.Get(), 0, num2DSubresources);

    ThrowIfFailed(_device->CreateCommittedResource
        (
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&_currUploadBuffer)
        ));
    ThrowIfFailed(_device->CreateCommittedResource
        (
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&_prevUploadBuffer)
        ));
    std::vector<float> initData(_numRows * _numCols, 0.0);
    for (int i = 0; i < initData.size(); i++)
        initData[i] = 0.0f;

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData.data();
    subResourceData.RowPitch = _numCols * sizeof(float);
    subResourceData.SlicePitch = subResourceData.RowPitch * _numRows;

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_prevSol.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources(cmdList, _prevSol.Get(), _prevUploadBuffer.Get(), 0, 0, num2DSubresources, &subResourceData);
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_prevSol.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_currSol.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources(cmdList, _currSol.Get(), _prevUploadBuffer.Get(), 0, 0, num2DSubresources, &subResourceData);
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_currSol.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
    
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_nextSol.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void GpuWaves::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

    uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

    _device->CreateShaderResourceView(_prevSol.Get(), &srvDesc, hCpuDescriptor);
    _device->CreateShaderResourceView(_currSol.Get(), &srvDesc, hCpuDescriptor.Offset(1, descriptorSize));
    _device->CreateShaderResourceView(_nextSol.Get(), &srvDesc, hCpuDescriptor.Offset(1, descriptorSize));

    _device->CreateUnorderedAccessView(_prevSol.Get(), nullptr, &uavDesc, hCpuDescriptor.Offset(1, descriptorSize));
    _device->CreateUnorderedAccessView(_currSol.Get(), nullptr, &uavDesc, hCpuDescriptor.Offset(1, descriptorSize));
    _device->CreateUnorderedAccessView(_nextSol.Get(), nullptr, &uavDesc, hCpuDescriptor.Offset(1, descriptorSize));

    _prevSolSrv = hGpuDescriptor;
    _currSolSrv = hGpuDescriptor.Offset(1, descriptorSize);
    _nextSolSrv = hGpuDescriptor.Offset(1, descriptorSize);

    _prevSolUav = hGpuDescriptor.Offset(1, descriptorSize);
    _currSolUav = hGpuDescriptor.Offset(1, descriptorSize);
    _nextSolUav = hGpuDescriptor.Offset(1, descriptorSize);
}

void GpuWaves::Update(const GameTimer& timer, ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSig, ID3D12PipelineState* pso)
{
    static float t = 0.0f;
    t += timer.DeltaTime();

    cmdList->SetPipelineState(pso);
    cmdList->SetComputeRootSignature(rootSig);

    if (t >= _timeStep)
    {
        cmdList->SetComputeRoot32BitConstants(0, 3, _K, 0);

        cmdList->SetComputeRootDescriptorTable(1, _prevSolUav);
        cmdList->SetComputeRootDescriptorTable(2, _currSolUav);
        cmdList->SetComputeRootDescriptorTable(3, _nextSolUav);

        UINT numGroupsX = _numCols / 16;
        UINT numGroupsY = _numRows / 16;
        cmdList->Dispatch(numGroupsX, numGroupsY, 1);

        auto resTemp = _prevSol;
        _prevSol = _currSol;
        _currSol = _nextSol;
        _nextSol = resTemp;

        auto srvTemp = _prevSolSrv;
        _prevSolSrv = _currSolSrv;
        _currSolSrv = _nextSolSrv;
        _nextSolSrv = srvTemp;

        auto uavTemp = _prevSolUav;
        _prevSolUav = _currSolUav;
        _currSolUav = _nextSolUav;
        _nextSolUav = uavTemp;

        t = 0.0f;
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_currSol.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
    }
}

void GpuWaves::Disturb(ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSig, ID3D12PipelineState* pso, UINT i, UINT j, float magnitude)
{
    cmdList->SetPipelineState(pso);
    cmdList->SetComputeRootSignature(rootSig);

    UINT disturbIndex[2] = { j, i };
    cmdList->SetComputeRoot32BitConstants(0, 1, &magnitude, 3);
    cmdList->SetComputeRoot32BitConstants(0, 2, disturbIndex, 4);

    cmdList->SetComputeRootDescriptorTable(3, _currSolUav);

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_currSol.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    cmdList->Dispatch(1, 1, 1);
}