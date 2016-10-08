//
// Helper class to render shadowmaps.
//

#pragma once

#include "../../Core/D3DUtil.h"

namespace DX12Samples
{
class ShadowMap
{
public:
    ShadowMap(ID3D12Device* device, UINT width, UINT height);
    ShadowMap(const ShadowMap& rhs) = delete;
    ShadowMap& operator= (const ShadowMap& rhs) = delete;
    ~ShadowMap() = default;
    /**
     * \brief Get shadow map width.
     */
    UINT Width() const;
    /**
     * \brief Get shadow map height.
     */
    UINT Height() const;
    /**
     * \brief Get shadow map resoruce.
     */
    ID3D12Resource* Resource();
    /**
     * \brief Get map shader resource view.
     */
    CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const;
    /**
     * \brief Get map depth stencil view.
     */
    CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;
    /**
     * \brief Get viewport to render shadowmap.
     */
    D3D12_VIEWPORT Viewport() const;
    /**
     * \brief Get scissor rect to render shadowmap.
     */
    D3D12_RECT ScissorRect() const;
    /**
     * \brief Build descriptors for shadowmap.
     * \param hCpuSrv CPU SRV in prebuilt heap.
     * \param hGpuSrv GPU SRV in prebuilt heap.
     * \param hCpuDsv CPU DSV in prebuilt heap.
     */
    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);
    /**
     * \brief Calls when window are resized to rebuild size dependent resources.
     */
    void OnResize(UINT newWidth, UINT newHeight);

private:
    void BuildDescriptors();
    void BuildResorce();

    ID3D12Device* _device = nullptr;
    D3D12_VIEWPORT _viewport;
    D3D12_RECT _scissorRect;

    UINT _width = 0;
    UINT _height = 0;
    DXGI_FORMAT _format = DXGI_FORMAT_R24G8_TYPELESS;

    CD3DX12_CPU_DESCRIPTOR_HANDLE _hCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _hGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE _hCpuDsv;

    Microsoft::WRL::ComPtr<ID3D12Resource> _shadowMap = nullptr;
};
}