#include "GaussBlurFilter.h"

namespace DX12Samples
{
GaussBlurFilter::GaussBlurFilter(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format)
{
    _device = device;
    _width = width;
    _height = height;
    _format = format;

    BuildResources();
}

ID3D12Resource* GaussBlurFilter::Output()
{
    return _blurMap0.Get();
}

void GaussBlurFilter::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize)
{
    _blur0CpuSrv = hCpuDescriptor;
    _blur0CpuUav = hCpuDescriptor.Offset(1, descriptorSize);
    _blur1CpuSrv = hCpuDescriptor.Offset(1, descriptorSize);
    _blur1CpuUav = hCpuDescriptor.Offset(1, descriptorSize);

    _blur0GpuSrv = hGpuDescriptor;
    _blur0GpuUav = hGpuDescriptor.Offset(1, descriptorSize);
    _blur1GpuSrv = hGpuDescriptor.Offset(1, descriptorSize);
    _blur1GpuUav = hGpuDescriptor.Offset(1, descriptorSize);

    BuildDescriptors();
}

void GaussBlurFilter::OnResize(UINT newWidth, UINT newHeight)
{
    if ((_width != newWidth) || (_height != newHeight))
    {
        _width = newWidth;
        _height = newHeight;

        BuildResources();
        BuildDescriptors();
    }
}

void GaussBlurFilter::Execute(ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSig, ID3D12PipelineState* horizBlurPSO, ID3D12PipelineState* vertBlurPSO, ID3D12Resource* input, int blurCount)
{
    auto weights = CalcGaussWeights(2.5f);
    int blurRadius = (int)weights.size() / 2;
    cmdList->SetComputeRootSignature(rootSig);

    cmdList->SetComputeRoot32BitConstants(0, 1, &blurRadius, 0);
    cmdList->SetComputeRoot32BitConstants(0, (UINT)weights.size(), weights.data(), 1);

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(input, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_blurMap0.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

    cmdList->CopyResource(_blurMap0.Get(), input);
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_blurMap0.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_blurMap1.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    for (int i = 0; i < blurCount; i++)
    {
        cmdList->SetPipelineState(horizBlurPSO);
        cmdList->SetComputeRootDescriptorTable(1, _blur0GpuSrv);
        cmdList->SetComputeRootDescriptorTable(2, _blur1GpuUav);

        UINT numGroupsX = (UINT)ceilf(_width / 256.0f);
        cmdList->Dispatch(numGroupsX, _height, 1);

        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_blurMap0.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_blurMap1.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
    
        cmdList->SetPipelineState(vertBlurPSO);
        cmdList->SetComputeRootDescriptorTable(1, _blur1GpuSrv);
        cmdList->SetComputeRootDescriptorTable(2, _blur0GpuUav);

        UINT numGroupsY = (UINT)ceilf(_height / 256.0f);
        cmdList->Dispatch(_width, numGroupsY, 1);

        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_blurMap0.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_blurMap1.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
    }
}

std::vector<float> GaussBlurFilter::CalcGaussWeights(float sigma)
{
    float twoSigma2 = 2.0f * sigma * sigma;

    int blurRadius = (int)ceil(2.0f * sigma);
    assert(blurRadius <= MaxBlurRadius);

    std::vector<float> weights;
    weights.resize(2 * blurRadius + 1);
    float weightSum = 0.0f;

    for (int i = -blurRadius; i <= blurRadius; i++)
    {
        float x = (float)i;
        weights[i + blurRadius] = expf(-x * x / twoSigma2);
        weightSum += weights[i + blurRadius];
    }

    for (int i = 0; i < weights.size(); i++)
        weights[i] /= weightSum;
    return weights;
}

void GaussBlurFilter::BuildDescriptors()
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

    _device->CreateShaderResourceView(_blurMap0.Get(), &srvDesc, _blur0CpuSrv);
    _device->CreateUnorderedAccessView(_blurMap0.Get(), nullptr, &uavDesc, _blur0CpuUav);

    _device->CreateShaderResourceView(_blurMap1.Get(), &srvDesc, _blur1CpuSrv);
    _device->CreateUnorderedAccessView(_blurMap1.Get(), nullptr, &uavDesc, _blur1CpuUav);
}

void GaussBlurFilter::BuildResources()
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
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&_blurMap0)
        ));

    ThrowIfFailed(_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&_blurMap1)
        ));
}
}