#include "CubeRenderTarget.h"
#include <DirectXPackedVector.h>

CubeRenderTarget::CubeRenderTarget(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format)
{
    _device = device;
    _width = width;
    _height = height;
    _format = format;

    _viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
    _scissorRect = { 0, 0, (int)width, (int)height };

    BuildResource();
}

ID3D12Resource* CubeRenderTarget::Resource()
{
    return _cubemap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CubeRenderTarget::Srv()
{
    return _hGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeRenderTarget::Rtv(int faceIndex)
{
    return _hCpuRtv[faceIndex];
}

D3D12_VIEWPORT CubeRenderTarget::Viewport() const
{
    return _viewport;
}

D3D12_RECT CubeRenderTarget::ScissorRect() const
{
    return _scissorRect;
}

void CubeRenderTarget::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv[6])
{
    _hCpuSrv = hCpuSrv;
    _hGpuSrv = hGpuSrv;

    for (int i = 0; i < 6; i++)
        _hCpuRtv[i] = hCpuRtv[i];
    BuildDescriptors();
}

void CubeRenderTarget::OnResize(UINT newWidth, UINT newHeight)
{
    if (_width != newWidth || _height != newHeight)
    {
        _width = newWidth;
        _height = newHeight;

        BuildResource();
        BuildDescriptors();
    }
}

void CubeRenderTarget::BuildDescriptors()
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = _format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.ResourceMinLODClamp = 0;

    _device->CreateShaderResourceView(_cubemap.Get(), &srvDesc, _hCpuSrv);

    for (int i = 0; i < 6; i++)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Format = _format;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.PlaneSlice = 0;

        rtvDesc.Texture2DArray.FirstArraySlice = i;
        rtvDesc.Texture2DArray.ArraySize = 1;

        _device->CreateRenderTargetView(_cubemap.Get(), &rtvDesc, _hCpuRtv[i]);
    }
}

void CubeRenderTarget::BuildResource()
{
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = _width;
    texDesc.Height = _height;
    texDesc.DepthOrArraySize = 6;
    texDesc.MipLevels = 1;
    texDesc.Format = _format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE optClear = {};
    optClear.Format = _format;
    optClear.Color[0] = DirectX::Colors::LightSteelBlue.f[0];
    optClear.Color[1] = DirectX::Colors::LightSteelBlue.f[1];
    optClear.Color[2] = DirectX::Colors::LightSteelBlue.f[2];
    optClear.Color[3] = DirectX::Colors::LightSteelBlue.f[3];
    
    ThrowIfFailed(_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_GENERIC_READ, &optClear, IID_PPV_ARGS(&_cubemap)));
}
