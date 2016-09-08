#pragma once

#include "SSAOFrameResource.h"

class SSAO
{
public:
    SSAO(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height);
    SSAO(const SSAO& rhs) = delete;
    SSAO& operator=(const SSAO& rhs) = delete;
    ~SSAO() = default;

    static const DXGI_FORMAT AmbientMapFormat = DXGI_FORMAT_R16_UNORM;
    static const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    static const int MaxBlurRadius = 5;

    UINT SSAOMapWidth() const;
    UINT SSAOMapHeight() const;

    void GetOffsetVectors(DirectX::XMFLOAT4 offsets[4]);
    std::vector<float> CalcGaussWeights(float sigma);

    ID3D12Resource* NormalMap();
    ID3D12Resource* AmbientMap();

    CD3DX12_CPU_DESCRIPTOR_HANDLE NormalMapRtv() const;
    CD3DX12_GPU_DESCRIPTOR_HANDLE NormalMapSrv() const;
    CD3DX12_GPU_DESCRIPTOR_HANDLE AmbientMapSrv() const;

    void BuildDescriptors(
        ID3D12Resource* depthStencilBuffer,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
        UINT cbvSrvUavDescriptorSize,
        UINT rtvDescriptorSize
        );

    void RebuildDescriptors(ID3D12Resource* depthStencilBuffer);

    void SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso);

    void OnResize(UINT newWidth, UINT newHeight);

    void ComputeSSAO(ID3D12GraphicsCommandList* cmdList, SSAOFrameResource* currFrame, int blurCount);
private:
    void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, SSAOFrameResource* currFrame, int blurCount);
    void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horizBlur);

    void BuildResources();
    void BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList);

    void BuildOffsetVectors();
private:
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

    DirectX::XMFLOAT4 _offsets[4];
    D3D12_VIEWPORT _viewport;
    D3D12_RECT _scissorRect;
};
