//
// Describes data which has to be set at each frame to GPU.
//

#pragma once

#include "../../Core/D3DUtil.h"
#include "../../Core/UploadBuffer.h"

namespace DX12Samples
{
struct StencilingFrameResource
{
    static const int NumFrameResources = 3;

    struct Vertex
    {
        Vertex() = default;
        Vertex(float x, float y, float z, float nx, float ny, float nz, float u, float v) :
            Pos(x, y, z),
            Normal(nx, ny, nz),
            TexC(u, v) {}
        DirectX::XMFLOAT3 Pos;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT2 TexC;
    };

    struct ObjectConstants
    {
        DirectX::XMFLOAT4X4 Model = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
    };

    struct PassConstants
    {
        DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 VP = MathHelper::Identity4x4();
        DirectX::XMFLOAT4X4 InvVP = MathHelper::Identity4x4();
        DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
        float cbPerObjectPad1 = 0.0f;
        DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
        DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0, 0.0f };
        float NearZ = 0.0f;
        float FarZ = 0.0f;
        float TotalTime = 0.0f;
        float DeltaTime = 0.0f;

        DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
        float FogStart = 5.0f;
        float FogRange = 150.0f;
        DirectX::XMFLOAT2 cbPerObjectPad2;

        Light Lights[MaxLights];
    };

    StencilingFrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
    StencilingFrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount, UINT waveVertCount) : StencilingFrameResource(device, passCount, objectCount, materialCount)
    {
        WavesVB = std::make_unique<UploadBuffer<Vertex>>(device, waveVertCount, false);
    }
    StencilingFrameResource(const StencilingFrameResource& rhs) = delete;
    StencilingFrameResource& operator= (const StencilingFrameResource& rhs) = delete;
    ~StencilingFrameResource();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

    std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
    std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

    std::unique_ptr<UploadBuffer<Vertex>> WavesVB = nullptr;

    UINT64 Fence = 0;

    static std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers()
    {
        const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
            0, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
            1, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
            2, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
            3, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
            4, // shaderRegister
            D3D12_FILTER_ANISOTROPIC, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
            0.0f,                             // mipLODBias
            8);                               // maxAnisotropy

        const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
            5, // shaderRegister
            D3D12_FILTER_ANISOTROPIC, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
            0.0f,                              // mipLODBias
            8);                                // maxAnisotropy

        return
        {
            pointWrap, pointClamp,
            linearWrap, linearClamp,
            anisotropicWrap, anisotropicClamp
        };
    }
};
}