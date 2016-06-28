#pragma once

#include <windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <cassert>
#include "d3dx12.h"
#include "DDSTextureLoader.h"
#include "MathHelper.h"

const int gNumFrameResources = 3;

inline void D3DSetDebugName(IDXGIObject* obj, const char* name)
{
    if (obj != nullptr)
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
}

inline void D3DSetDebugName(ID3D12Device* obj, const char* name)
{
    if (obj != nullptr)
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
}

inline void D3DSetDebugName(ID3D12DeviceChild* obj, const char* name)
{
    if (obj != nullptr)
        obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
}

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

class D3DUtil
{
public:
    static bool IsKeyDown(int keyCode);
    static std::string ToString(HRESULT hr);
    static UINT CalcConstantBufferByteSize(UINT byteSize)
    {
        return (byteSize + 255) & ~255;
    }

    static Microsoft::WRL::ComPtr<ID3DBlob> LoadBinary(const std::wstring& filename);

    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer
        (
            ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
            const void* initData, UINT64 byteSize,
            Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer
        );
    static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader
        (
            const std::wstring& fileName,
            const D3D_SHADER_MACRO* defines,
            const std::string& entryPoint,
            const std::string& target
        );
};

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString() const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring FileName;
    int LineNumber = -1;
};

struct SubmeshGeometry
{
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    INT BaseVertexLocation = 0;

    DirectX::BoundingBox Bounds;
};

struct MeshGeometry
{
    std::string Name;

    Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPUSlot2 = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPUSlot2 = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploaderSlot2 = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

    UINT VertexByteStride = 0;
    UINT VertexBufferByteSize = 0;

    UINT VertexByteStrideSlot2 = 0;
    UINT VertexBufferByteSizeSlot2 = 0;


    DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
    UINT IndexBufferByteSize = 0;

    std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

    D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
        vbv.StrideInBytes = VertexByteStride;
        vbv.SizeInBytes = VertexBufferByteSize;
        return vbv;
    }

    D3D12_VERTEX_BUFFER_VIEW VertexBufferViewSlot2() const
    {
        D3D12_VERTEX_BUFFER_VIEW vbv;
        vbv.BufferLocation = VertexBufferGPUSlot2->GetGPUVirtualAddress();
        vbv.StrideInBytes = VertexByteStrideSlot2;
        vbv.SizeInBytes = VertexBufferByteSizeSlot2;
        return vbv;
    }

    D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
    {
        D3D12_INDEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
        ibv.Format = IndexFormat;
        ibv.SizeInBytes = IndexBufferByteSize;
        return ibv;
    }

    void DisposeUploaders()
    {
        VertexBufferUploader = nullptr;
        VertexBufferUploaderSlot2 = nullptr;
        IndexBufferUploader = nullptr;
    }
};

struct Light
{
    DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
    float FalloffStart = 1.0f;
    DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
    float FalloffEnd = 10.0f;
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
    float SpotPower = 64.0f;
};

#define MaxLights 16

struct MaterialConstants
{
    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;

    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct Material
{
    std::string Name;
    int MatCBIndex = -1;
    int DiffuseSrvHeapIndex = -1;
    int NormalSrvHeapIndex = -1;
    int NumFramesDirty = gNumFrameResources;

    DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;
    DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct Texture
{
    std::string Name;
    std::wstring Filename;

    Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if (FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); }\
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if (x) { x->Release(); x = 0; }}
#endif