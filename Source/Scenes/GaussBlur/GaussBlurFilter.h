//
// Applies gaussian blur to image.
//

#pragma once

#include "../../Core/D3DUtil.h"

namespace DX12Samples
{
class GaussBlurFilter
{
public:
    GaussBlurFilter(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
    GaussBlurFilter(const GaussBlurFilter& rhs) = delete;
    GaussBlurFilter& operator=(const GaussBlurFilter& rhs) = delete;
    ~GaussBlurFilter() = default;
    /**
     * \brief Get blurred resource.
     */
    ID3D12Resource* Output();
    /**
     * \brief Build descriptors for blur.
     * \param hCpuDescriptor Cpu descriptor in prebuilt heap.
     * \param hGpuDescriptor Gpu descriptor in prebuilt heap.
     * \param descriptorSize Size of CBV SRV UAV decrtiptor to offset in heap.
     */
    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize);
    /**
    * \brief Calls when window are resized to rebuild size dependent resources.
    */
    void OnResize(UINT newWidth, UINT newHeight);
    /**
     * \brief Execute blur for input resorce.
     * \param cmdList Command list for execution.
     * \param rootSig Compute root signature for blur.
     * \param horizBlurPSO PSO for horizontal blur.
     * \param vertBlurPSO PSO for vertical blur.
     * \param input Input resource to perform blur.
     * \param blurCount Count of blur iterations.
     */
    void Execute(ID3D12GraphicsCommandList* cmdList,
        ID3D12RootSignature* rootSig,
        ID3D12PipelineState* horizBlurPSO,
        ID3D12PipelineState* vertBlurPSO,
        ID3D12Resource* input,
        int blurCount);

private:
    /**
     * \brief Calculate Gauss weight for sigma.
     */
    std::vector<float> CalcGaussWeights(float sigma);
    void BuildDescriptors();
    void BuildResources();

    const int MaxBlurRadius = 5;
    ID3D12Device* _device = nullptr;

    UINT _width = 0;
    UINT _height = 0;
    DXGI_FORMAT _format = DXGI_FORMAT_R8G8B8A8_UNORM;

    CD3DX12_CPU_DESCRIPTOR_HANDLE _blur0CpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE _blur0CpuUav;

    CD3DX12_CPU_DESCRIPTOR_HANDLE _blur1CpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE _blur1CpuUav;

    CD3DX12_GPU_DESCRIPTOR_HANDLE _blur0GpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _blur0GpuUav;

    CD3DX12_GPU_DESCRIPTOR_HANDLE _blur1GpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _blur1GpuUav;

    Microsoft::WRL::ComPtr<ID3D12Resource> _blurMap0 = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _blurMap1 = nullptr;
};
}