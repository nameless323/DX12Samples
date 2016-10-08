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

    CD3DX12_GPU_DESCRIPTOR_HANDLE OutputSrv();

    UINT DescriptorCount() const;

    void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize);

    void OnResize(UINT newWidth, UINT newHeight);

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