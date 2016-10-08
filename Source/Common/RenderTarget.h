//
// Describes render target.
//

#pragma once

#include "../../Core/D3DUtil.h"

namespace DX12Samples
{
class RenderTarget
{
public:
    /**
     * \brief Create render target resource.
     * \param device which will be used for creation.
     * \param width texture width.
     * \param height texture height.
     * \param format Render target texture format.
     */
    RenderTarget(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
    RenderTarget(const RenderTarget& rhs) = delete;
    RenderTarget& operator= (const RenderTarget& rhs) = delete;
    ~RenderTarget() = default;
    /**
     * \brief Get render target resource.
     */
    ID3D12Resource* Resource();
    /**
     * \brief Get render target shader resource view.
     */
    CD3DX12_GPU_DESCRIPTOR_HANDLE Srv();
    /**
     * \brief Get render target view.
     */
    CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv();
    /**
     * \brief Build descriptors for render target.
     * \param hCpuSrv Cpu descriptor for render target in SRV heap.
     * \param hGpuSrv Gpu descriptor for render target in SRV heap.
     * \param hCpuRtv descriptor for render target in SRV RTV.
     */
    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv);
    /**
     * \brief Call when application window are resized. Rebuilds resource according to new width and height.
     */
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
}