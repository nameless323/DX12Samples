//
// Class which execute sobel filter on image.
//

#pragma once

#include "../../Core/D3DUtil.h"

namespace DX12Samples
{
class SobelFilter
{
public:
    SobelFilter(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
    SobelFilter(const SobelFilter& rhs) = delete;
    SobelFilter& operator= (const SobelFilter& rhs) = delete;
    ~SobelFilter() = default;
    /**
     * \brief Srv for filter texture.
     */
    CD3DX12_GPU_DESCRIPTOR_HANDLE OutputSrv();
    /**
     * \brief Get nessesary count for decriptors.
     */
    UINT DescriptorCount() const;
    /**
     * \brief Build descriptors for sobel filter.
     * \param hCpuDescriptor CPU decriptor in prebuild heap.
     * \param hGpuDescriptor GPU decriptor in prebuild heap. 
     * \param descriptorSize CBV SRV UAB descriptor size to offset in heap.
     */
    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize);
    /**
     * \brief Calls when window are resized to rebuild size dependent resources.
     */
    void OnResize(UINT newWidth, UINT newHeight);
    /**
     * \brief Execute sobel filter on input resorce. 
     */
    void Execute(ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSignature, ID3D12PipelineState* pso, CD3DX12_GPU_DESCRIPTOR_HANDLE input);

private:
    void BuildDescriptors();
    void BuildResource();

    ID3D12Device* _device = nullptr;

    UINT _width = 0;
    UINT _height = 0;
    DXGI_FORMAT _format = DXGI_FORMAT_R8G8B8A8_UNORM;

    CD3DX12_CPU_DESCRIPTOR_HANDLE _cpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE _cpuUav;

    CD3DX12_GPU_DESCRIPTOR_HANDLE _gpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _gpuUav;

    Microsoft::WRL::ComPtr<ID3D12Resource> _output = nullptr;
};
}