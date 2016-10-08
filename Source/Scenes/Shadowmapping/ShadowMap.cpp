#include "ShadowMap.h"

namespace DX12Samples
{
ShadowMap::ShadowMap(ID3D12Device* device, UINT width, UINT height)
{
    _device = device;
    _width = width;
    _height = height;

    _viewport = { 0.0f, 0.0f, (float)_width, (float)_height, 0.0f, 1.0f };
    _scissorRect = { 0, 0, (int)_width, (int)_height };

    BuildResorce();
}

UINT ShadowMap::Width() const
{
    return _width;
}

UINT ShadowMap::Height() const
{
    return _height;
}

ID3D12Resource* ShadowMap::Resource()
{
    return _shadowMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE ShadowMap::Srv() const
{
    return _hGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowMap::Dsv() const
{
    return _hCpuDsv;
}

D3D12_VIEWPORT ShadowMap::Viewport() const
{
    return _viewport;
}

D3D12_RECT ShadowMap::ScissorRect() const
{
    return _scissorRect;
}

void ShadowMap::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
    _hCpuSrv = hCpuSrv;
    _hGpuSrv = hGpuSrv;
    _hCpuDsv = hCpuDsv;

    BuildDescriptors();
}

void ShadowMap::OnResize(UINT newWidth, UINT newHeight)
{
    if (_width != newWidth || _height != newHeight)
    {
        _width = newWidth;
        _height = newHeight;

        BuildResorce();
        BuildDescriptors();
    }

}

void ShadowMap::BuildDescriptors()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    srvDesc.Texture2D.PlaneSlice = 0;
    _device->CreateShaderResourceView(_shadowMap.Get(), &srvDesc, _hCpuSrv);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    _device->CreateDepthStencilView(_shadowMap.Get(), &dsvDesc, _hCpuDsv);
}

void ShadowMap::BuildResorce()
{
    D3D12_RESOURCE_DESC texDesc = {};
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
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear = {};
    optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    ThrowIfFailed(_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(_shadowMap.GetAddressOf())
        ));
}
}