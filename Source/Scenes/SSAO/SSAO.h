//
// Creates SSAO map for resorce.
//

#pragma once

#include "SSAOFrameResource.h"

namespace DX12Samples
{
class SSAO
{
public:
    SSAO(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);
    SSAO(const SSAO& rhs) = delete;
    SSAO& operator=(const SSAO& rhs) = delete;
    ~SSAO() = default;

    /**
     * \brief Get ssao map texture width.
     */
    UINT SSAOMapWidth() const;
    /**
     * \brief Get ssao map texture height.
     */
    UINT SSAOMapHeight() const;
    /**
     * \brief Get 14 offset vectors.
     */
    void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);
    /**
     * \brief Calculates gauss weights for sigma.
     */
    std::vector<float> CalcGaussWeights(float sigma);
    /**
     * \brief Get normal map resoruce.
     */
    ID3D12Resource* NormalMap();
    /**
     * \brief Get SSAO map resource.
     */
    ID3D12Resource* AmbientMap();
    /**
     * \brief Get normal map render target view.
     */
    CD3DX12_CPU_DESCRIPTOR_HANDLE NormalMapRtv() const;
    /**
     * \brief Get normal map shader resource view.
     */
    CD3DX12_GPU_DESCRIPTOR_HANDLE NormalMapSrv() const;
    /**
     * \brief Get SSAO map shader resource view.
     */
    CD3DX12_GPU_DESCRIPTOR_HANDLE AmbientMapSrv() const;
    /**
     * \brief Build ssao decriptors.
     * \param depthStencilBuffer Depth scene resource.
     * \param hCpuSrv CPU shader resource view in prebuild srv heap.
     * \param hGpuSrv GPU shader resource view in prebuild srv heap.
     * \param hCpuRtv CPU render target view in prebuild rtv heap.
     * \param cbvSrvUavDescriptorSize SBV SRV UAV descriptor size to offset in heap.
     * \param rtvDescriptorSize RTV descriptor size to offset in rtv heap.
     */
    void BuildDescriptors(
        ID3D12Resource* depthStencilBuffer,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
        UINT cbvSrvUavDescriptorSize,
        UINT rtvDescriptorSize
        );
    /**
     * \brief Rebuild descriptors.
     */
    void RebuildDescriptors(ID3D12Resource* depthStencilBuffer);
    /**
     * \brief Set piplene state objects.
     * \param ssaoPso PSO for ssao.
     * \param ssaoBlurPso PSO for making blur for SSAO map.
     */
    void SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso);
    /**
    * \brief Calls when window are resized to rebuild size dependent resources.
    */
    void OnResize(UINT newWidth, UINT newHeight);
    /**
     * \brief Compute SSAO map. 
     */
    void ComputeSSAO(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS currFrame, int blurCount);

    static const DXGI_FORMAT AmbientMapFormat = DXGI_FORMAT_R16_UNORM;
    static const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static const int MaxBlurRadius = 5;

private:
    void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS currFrame, int blurCount);
    void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horizBlur);
    void BuildResources();
    void BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList);
    void BuildOffsetVectors();

    ID3D12Device* _device;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> _ssaoRootSig;

    ID3D12PipelineState* _ssaoPso = nullptr;
    ID3D12PipelineState* _blurPso = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> _randomVectorMap;
    Microsoft::WRL::ComPtr<ID3D12Resource> _randomVectorMapUploadBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> _normalMap;
    Microsoft::WRL::ComPtr<ID3D12Resource> _ambientMap0;
    Microsoft::WRL::ComPtr<ID3D12Resource> _ambientMap1;

    CD3DX12_CPU_DESCRIPTOR_HANDLE _hNormalMapCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _hNormalMapGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE _hNormalMapCpuRtv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE _depthMapCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _depthMapGpuSrv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE _randomVectorMapCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _randomVectorMapGpuSrv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE _hAmbientMap0CpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _hAmbientMap0GpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE _hAmbientMap0CpuRtv;

    CD3DX12_CPU_DESCRIPTOR_HANDLE _hAmbientMap1CpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE _hAmbientMap1GpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE _hAmbientMap1CpuRtv;

    UINT _renderTargetWidth;
    UINT _renderTargetHeight;

    DirectX::XMFLOAT4 _offsets[14];
    D3D12_VIEWPORT _viewport;
    D3D12_RECT _scissorRect;
};
}