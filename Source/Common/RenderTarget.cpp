#include "RenderTarget.h"

namespace DX12Samples
{
RenderTarget::RenderTarget(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format)
{
    _device = device;

    _width = width;
    _height = height;
    _format = format;

    BuildResource();
}

ID3D12Resource* RenderTarget::Resource()
{
    return _offscreenTex.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE RenderTarget::Srv()
{
    return _hGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE RenderTarget::Rtv()
{
    return _hCpuRtv;
}

void RenderTarget::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv)
{
    _hCpuSrv = hCpuSrv;
    _hGpuSrv = hGpuSrv;
    _hCpuRtv = hCpuRtv;

    BuildDescriptors();
}

void RenderTarget::OnResize(UINT newWidth, UINT newHeight)
{
    if ((_width != newWidth) || (_height != newHeight))
    {
        _width = newWidth;
        _height = newHeight;

        BuildResource();
        BuildDescriptors();
    }
}

void RenderTarget::BuildDescriptors()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = _format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    _device->CreateShaderResourceView(_offscreenTex.Get(), &srvDesc, _hCpuSrv);
    _device->CreateRenderTargetView(_offscreenTex.Get(), nullptr, _hCpuRtv);
}

void RenderTarget::BuildResource()
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
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    ThrowIfFailed(_device->CreateCommittedResource
        (
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_offscreenTex)
        ));
}
}