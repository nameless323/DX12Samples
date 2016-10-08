#include "SSAO.h"

#include <DirectXPackedVector.h>
#include <minwinbase.h>

namespace DX12Samples
{
using namespace DirectX;
using namespace PackedVector;
using namespace Microsoft::WRL;

SSAO::SSAO(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height)
{
    _device = device;
    OnResize(width, height);

    BuildOffsetVectors();
    BuildRandomVectorTexture(cmdList);
}

UINT SSAO::SSAOMapWidth() const
{
    return _renderTargetWidth / 2;
}

UINT SSAO::SSAOMapHeight() const
{
    return _renderTargetHeight / 2;
}

void SSAO::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14])
{
        memcpy(offsets, _offsets, sizeof(offsets[0]) * 14);
    //    std::copy(&_offsets[0], &_offsets[14], &offsets[0]);
//    for (int i = 0; i < 14; i++)
//        offsets[i] = _offsets[i];
}

std::vector<float> SSAO::CalcGaussWeights(float sigma)
{
    float twoSigma2 = 2.0f * sigma * sigma;
    int blurRadius = static_cast<int>(ceil(2.0f * sigma));

    assert(blurRadius <= MaxBlurRadius);
    std::vector<float> weights;
    weights.resize(2 * blurRadius + 1);

    float weightSum = 0.0f;

    for (int i = -blurRadius; i <= blurRadius; i++)
    {
        float x = (float)i;
        weights[i + blurRadius] = expf(-x * x / twoSigma2);
        weightSum += weights[i + blurRadius];
    }

    for (int i = 0; i < weights.size(); i++)
    {
        weights[i] /= weightSum;
    }
    return weights;
}

ID3D12Resource* SSAO::NormalMap()
{
    return _normalMap.Get();
}

ID3D12Resource* SSAO::AmbientMap()
{
    return _ambientMap0.Get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SSAO::NormalMapRtv() const
{
    return _hNormalMapCpuRtv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SSAO::NormalMapSrv() const
{
    return _hNormalMapGpuSrv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SSAO::AmbientMapSrv() const
{
    return _hAmbientMap0GpuSrv;
}

void SSAO::BuildDescriptors(ID3D12Resource* depthStencilBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv, UINT cbvSrvUavDescriptorSize, UINT rtvDescriptorSize)
{
    _hAmbientMap0CpuSrv = hCpuSrv;
    _hAmbientMap1CpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    _hNormalMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    _depthMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    _randomVectorMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    _hAmbientMap0GpuSrv = hGpuSrv;
    _hAmbientMap1GpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    _hNormalMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    _depthMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    _randomVectorMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    _hNormalMapCpuRtv = hCpuRtv;
    _hAmbientMap0CpuRtv = hCpuRtv.Offset(1, rtvDescriptorSize);
    _hAmbientMap1CpuRtv = hCpuRtv.Offset(1, rtvDescriptorSize);

    RebuildDescriptors(depthStencilBuffer);
}

void SSAO::RebuildDescriptors(ID3D12Resource* depthStencilBuffer)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = NormalMapFormat;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    _device->CreateShaderResourceView(_normalMap.Get(), &srvDesc, _hNormalMapCpuSrv);

    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    _device->CreateShaderResourceView(depthStencilBuffer, &srvDesc, _depthMapCpuSrv);

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    _device->CreateShaderResourceView(_randomVectorMap.Get(), &srvDesc, _randomVectorMapCpuSrv);

    srvDesc.Format = AmbientMapFormat;
    _device->CreateShaderResourceView(_ambientMap0.Get(), &srvDesc, _hAmbientMap0CpuSrv);
    _device->CreateShaderResourceView(_ambientMap1.Get(), &srvDesc, _hAmbientMap1CpuSrv);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = NormalMapFormat;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;
    _device->CreateRenderTargetView(_normalMap.Get(), &rtvDesc, _hNormalMapCpuRtv);

    rtvDesc.Format = AmbientMapFormat;
    _device->CreateRenderTargetView(_ambientMap0.Get(), &rtvDesc, _hAmbientMap0CpuRtv);
    _device->CreateRenderTargetView(_ambientMap1.Get(), &rtvDesc, _hAmbientMap1CpuRtv);
}

void SSAO::SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso)
{
    _ssaoPso = ssaoPso;
    _blurPso = ssaoBlurPso;
}

void SSAO::OnResize(UINT newWidth, UINT newHeight)
{
    if (_renderTargetWidth != newWidth || _renderTargetHeight != newHeight)
    {
        _renderTargetWidth = newWidth;
        _renderTargetHeight = newHeight;

        _viewport.TopLeftX = 0.0f;
        _viewport.TopLeftY = 0.0f;
        _viewport.Width = _renderTargetWidth / 2.0f;
        _viewport.Height = _renderTargetHeight / 2.0f;
        _viewport.MinDepth = 0.0f;
        _viewport.MaxDepth = 1.0f;

        _scissorRect = {0, 0, (int)_renderTargetWidth / 2, (int)_renderTargetHeight / 2};
        BuildResources();
    }
}

void SSAO::ComputeSSAO(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS currFrame, int blurCount)
{
    cmdList->RSSetViewports(1, &_viewport);
    cmdList->RSSetScissorRects(1, &_scissorRect);

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_ambientMap0.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

    float clearValue[] = {1.0f, 1.0f, 1.0f, 1.0f};
    cmdList->ClearRenderTargetView(_hAmbientMap0CpuRtv, clearValue, 0, nullptr);

    cmdList->OMSetRenderTargets(1, &_hAmbientMap0CpuRtv, true, nullptr);

    cmdList->SetGraphicsRootConstantBufferView(0, currFrame);
    cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);

    cmdList->SetGraphicsRootDescriptorTable(2, _hNormalMapGpuSrv);
    cmdList->SetGraphicsRootDescriptorTable(3, _randomVectorMapGpuSrv);
    cmdList->SetPipelineState(_ssaoPso);

    cmdList->IASetVertexBuffers(0, 0, nullptr);
    cmdList->IASetIndexBuffer(nullptr);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(6, 1, 0, 0);

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_ambientMap0.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

    BlurAmbientMap(cmdList, currFrame, blurCount);
}

void SSAO::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, D3D12_GPU_VIRTUAL_ADDRESS currFrame, int blurCount)
{
    cmdList->SetPipelineState(_blurPso);
    cmdList->SetGraphicsRootConstantBufferView(0, currFrame);

    for (int i = 0; i < blurCount; i++)
    {
        BlurAmbientMap(cmdList, true);
        BlurAmbientMap(cmdList, false);
    }
}

void SSAO::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horizBlur)
{
    ID3D12Resource* output = nullptr;
    CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

    if (horizBlur)
    {
        output = _ambientMap1.Get();
        inputSrv = _hAmbientMap0GpuSrv;
        outputRtv = _hAmbientMap1CpuRtv;
        cmdList->SetGraphicsRoot32BitConstant(1, 1, 0);
    }
    else
    {
        output = _ambientMap0.Get();
        inputSrv = _hAmbientMap1GpuSrv;
        outputRtv = _hAmbientMap0CpuRtv;
        cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);
    }
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

    float clearValue[] = {1.0f, 1.0f, 1.0f, 1.0f};
    cmdList->ClearRenderTargetView(outputRtv, clearValue, 0, nullptr);

    cmdList->OMSetRenderTargets(1, &outputRtv, true, nullptr);

    cmdList->SetGraphicsRootDescriptorTable(2, _hNormalMapGpuSrv);
    cmdList->SetGraphicsRootDescriptorTable(3, inputSrv);

    cmdList->IASetVertexBuffers(0, 0, nullptr);
    cmdList->IASetIndexBuffer(nullptr);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(6, 1, 0, 0);
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SSAO::BuildResources()
{
    _normalMap = nullptr;
    _ambientMap0 = nullptr;
    _ambientMap1 = nullptr;

    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = _renderTargetWidth;
    texDesc.Height = _renderTargetHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = NormalMapFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    float normalClearColor[] = {0.0f, 0.0f, 1.0f, 0.0f};
    CD3DX12_CLEAR_VALUE optClear(NormalMapFormat, normalClearColor);
    ThrowIfFailed(_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(&_normalMap)));

    texDesc.Width = _renderTargetWidth / 2;
    texDesc.Height = _renderTargetHeight / 2;
    texDesc.Format = AmbientMapFormat;
    float ambientClearColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    optClear = CD3DX12_CLEAR_VALUE(AmbientMapFormat, ambientClearColor);

    ThrowIfFailed(_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&_ambientMap0)));

    ThrowIfFailed(_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        &optClear,
        IID_PPV_ARGS(&_ambientMap1)));
}

void SSAO::BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList)
{
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = 256;
    texDesc.Height = 256;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    ThrowIfFailed(_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_randomVectorMap)));

    const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(_randomVectorMap.Get(), 0, num2DSubresources);

    ThrowIfFailed(_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(_randomVectorMapUploadBuffer.GetAddressOf())));

    XMCOLOR initData[256 * 256];
    for (int i = 0; i < 256; i++)
    {
        for (int j = 0; j < 256; j++)
        {
            XMFLOAT3 v(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF());
            initData[i * 256 + j] = XMCOLOR(v.x, v.y, v.z, 0.0f);
        }
    }
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = 256 * sizeof(XMCOLOR);
    subResourceData.SlicePitch = subResourceData.RowPitch * 256;

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_randomVectorMap.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources(cmdList, _randomVectorMap.Get(), _randomVectorMapUploadBuffer.Get(), 0, 0, num2DSubresources, &subResourceData);
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_randomVectorMap.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SSAO::BuildOffsetVectors()
{
    _offsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
    _offsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

    _offsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
    _offsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

    _offsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
    _offsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

    _offsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
    _offsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);


    _offsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
    _offsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

    _offsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
    _offsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

    _offsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
    _offsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

    for (int i = 0; i < 14; ++i)
    {
        float s = MathHelper::RandF(0.25f, 1.0f);

        XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&_offsets[i]));

        XMStoreFloat4(&_offsets[i], v);
    }
}
}