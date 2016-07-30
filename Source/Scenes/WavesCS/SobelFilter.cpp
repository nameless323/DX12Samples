#include "SobelFilter.h"

SobelFilter::SobelFilter(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format)
{
    _device = device;
    _width = width;
    _height = height;
    _format = format;

    BuildResource();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SobelFilter::OutputSrv()
{
    return _gpuSrv;
}

UINT SobelFilter::DescriptorCount() const
{
    return 2;
}

void SobelFilter::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize)
{
    _cpuSrv = hCpuDescriptor;
    _cpuUav = hCpuDescriptor.Offset(1, descriptorSize);
    _gpuSrv = hGpuDescriptor;
    _gpuUav = hGpuDescriptor.Offset(1, descriptorSize);

    BuildDescriptors();
}

void SobelFilter::OnResize(UINT newWidth, UINT newHeight)
{
    if (_width != newWidth || _height != newHeight)
    {
        _width = newWidth;
        _height = newHeight;

        BuildResource();
        BuildDescriptors();
    }
}

void SobelFilter::Execute(ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSignature, ID3D12PipelineState* pso, CD3DX12_GPU_DESCRIPTOR_HANDLE input)
{
    cmdList->SetComputeRootSignature(rootSignature);
    cmdList->SetPipelineState(pso);

    cmdList->SetComputeRootDescriptorTable(0, input);
    cmdList->SetComputeRootDescriptorTable(2, _gpuUav);

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_output.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    UINT numGroupsX = (UINT)ceilf(_width / 16.0f);
    UINT numGroupsY = (UINT)ceilf(_height / 16.0f);
    cmdList->Dispatch(numGroupsX, numGroupsY, 1);

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SobelFilter::BuildDescriptors()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = _format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = _format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

    _device->CreateShaderResourceView(_output.Get(), &srvDesc, _cpuSrv);
    _device->CreateUnorderedAccessView(_output.Get(), nullptr, &uavDesc, _cpuUav);
}

void SobelFilter::BuildResource()
{
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = _width;
    texDesc.Height = _height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = _format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ThrowIfFailed(_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_output)));
}
