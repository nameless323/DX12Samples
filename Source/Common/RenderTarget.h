#pragma once

#include "../../Core/D3DUtil.h"

class RenderTarget
{
public:
    RenderTarget(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
    RenderTarget(const RenderTarget& rhs) = delete;
    RenderTarget& operator= (const RenderTarget& rhs) = delete;
    ~RenderTarget() = default;

    ID3D12Resource* Resource();
    CD3DX12_GPU_DESCRIPTOR_HANDLE Srv();
    CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv();

    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv);
    void OnResize(UINT newWidth, UINT newHeight);
private:
    void BuildDescriptors();
    void BuildResource();

    ID3D12Device* _device = nullptr;
    UINT _width = 0;
    UINT _height = 0;
    DXGI_FORMAT _format = DXGI_FORMAT_R8G8B8A8_UNORM;

    CD3DX12_CPU_DESCRIPTOR_HANDLE _hCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _hGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE _hCpuRtv;

    Microsoft::WRL::ComPtr<ID3D12Resource> _offscreenTex = nullptr;
};
