#pragma once

#include "../../Core/D3DUtil.h"

class CubeRenderTarget
{
public:
    CubeRenderTarget(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);

    CubeRenderTarget(const CubeRenderTarget& rhs) = delete;
    CubeRenderTarget& operator=(const CubeRenderTarget& rhs) = delete;
    ~CubeRenderTarget() = default;

    ID3D12Resource* Resource();
    CD3DX12_GPU_DESCRIPTOR_HANDLE Srv();
    CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv(int faceIndex);

    D3D12_VIEWPORT Viewport() const;
    D3D12_RECT ScissorRect() const;

    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv[6]);
    void OnResize(UINT newWidth, UINT newHeight);
private:
    void BuildDescriptors();
    void BuildResource();

private:
    ID3D12Device* _device = nullptr;
    D3D12_VIEWPORT _viewport;
    D3D12_RECT _scissorRect;

    UINT _width = 0;
    UINT _height = 0;
    DXGI_FORMAT _format = DXGI_FORMAT_R8G8B8A8_UNORM;
    CD3DX12_CPU_DESCRIPTOR_HANDLE _hCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _hGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE _hCpuRtv[6];

    Microsoft::WRL::ComPtr<ID3D12Resource> _cubemap = nullptr;
public:
    enum class CubemapFace : int
    {
        PositiveX = 0,
        NegativeX = 1,
        PositiveY = 2,
        NegativeY = 3,
        PositiveZ = 4,
        NegativeZ = 5
    };
};