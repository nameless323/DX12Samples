//
// Render target for dynamic cubemap.
//

#pragma once

#include "../../Core/D3DUtil.h"

namespace DX12Samples
{
class CubeRenderTarget
{
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

    CubeRenderTarget(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
    CubeRenderTarget(const CubeRenderTarget& rhs) = delete;
    CubeRenderTarget& operator=(const CubeRenderTarget& rhs) = delete;
    ~CubeRenderTarget() = default;
    /**
     * \brief Get render target resource.
     */
    ID3D12Resource* Resource();
    /**
     * \brief Get shader resource view for cubemap.
     */
    CD3DX12_GPU_DESCRIPTOR_HANDLE Srv();
    /**
     * \brief Get render target view for specific cubemap face.
     */
    CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv(int faceIndex);
    /**
     * \brief Get viewport to render cubemap face.
     */
    D3D12_VIEWPORT Viewport() const;
    /**
     * \brief Get scissor rect to render cubemap face.
     */
    D3D12_RECT ScissorRect() const;
    /**
     * \brief Build descriptors for cubemap.
     * \param hCpuSrv Cpu Srv in prebuilt heap.
     * \param hGpuSrv Gpu Srv in prebuild heap.
     * \param hCpuRtv Six render targets view - one for each face of a cube.
     */
    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv[6]);
    /**
     * \brief Calls when window are resized to rebuild size dependent resources.
     */
    void OnResize(UINT newWidth, UINT newHeight);

private:
    void BuildDescriptors();
    void BuildResource();

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
};
}