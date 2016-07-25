#include "CsVecLen.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;

CsVecLen::CsVecLen(HINSTANCE hInstance) : Application(hInstance)
{
    
}

CsVecLen::~CsVecLen()
{
    if (_device != nullptr)
        FlushCommandQueue();
}

bool CsVecLen::Init()
{
    if (!Application::Init())
        return false;
    ThrowIfFailed(_commandAllocator->Reset());
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    BuildBuffers();
    BuildShaderAndInputLayout();
    BuildRootSignature();
    BuildPSOs();

    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* lists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(1, lists);
    FlushCommandQueue();

    DoComputeWork();
    return true;
}

LRESULT CsVecLen::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int CsVecLen::Run()
{
    return Application::Run();
}

void CsVecLen::OnResize()
{
    Application::OnResize();
}

void CsVecLen::Update(const GameTimer& timer)
{
}

void CsVecLen::Draw(const GameTimer& timer)
{
    FlushCommandQueue();
}

void CsVecLen::DoComputeWork()
{
    ThrowIfFailed(_commandAllocator->Reset());
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), _PSOs["vecLen"].Get()));
    _commandList->SetComputeRootSignature(_rootSignature.Get());

    _commandList->SetComputeRootShaderResourceView(0, _inputBuffer->GetGPUVirtualAddress());
    _commandList->SetComputeRootUnorderedAccessView(1, _outputBuffer->GetGPUVirtualAddress());

    _commandList->Dispatch(1, 1, 1);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(_outputBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    
    _commandList->ResourceBarrier(1, &barrier);
    _commandList->CopyResource(_readBackBuffer.Get(), _outputBuffer.Get());
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(_outputBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    _commandList->ResourceBarrier(1, &barrier);
    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* lists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(1, lists);
    FlushCommandQueue();

    float* data;
    _readBackBuffer->Map(0, nullptr, (void**)&data);
    std::ofstream out("lenOut.txt");
    for (int i = 0; i < NumDataElements; i++)
    {
        out << data[i] << std::endl;
    }
    out.close();
    _readBackBuffer->Unmap(0, nullptr);
}

void CsVecLen::BuildBuffers()
{
    std::array<XMFLOAT3, NumDataElements> data;
    std::ofstream out("lenIn.txt");
    for (int i = 0; i < NumDataElements; i++)
    {
        XMStoreFloat3(&data[i], MathHelper::RandUnitVec3() * MathHelper::RandF(1, 15));
        out << "(" << data[i].x << ", " << data[i].y << ", " << data[i].z << ")" << std::endl;
    }
    out.close();

    UINT inData = sizeof(XMFLOAT3) * NumDataElements;
    UINT outData = sizeof(float) * NumDataElements;

    _inputBuffer = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), data.data(), inData, _inputUploadBuffer);

    _device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), 
        D3D12_HEAP_FLAG_NONE, 
        &CD3DX12_RESOURCE_DESC::Buffer(outData, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), 
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 
        nullptr, 
        IID_PPV_ARGS(_outputBuffer.GetAddressOf()));

    _device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(outData),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(_readBackBuffer.GetAddressOf())
        );
}

void CsVecLen::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER p[2];
    p[0].InitAsShaderResourceView(0);
    p[1].InitAsUnorderedAccessView(0);

    CD3DX12_ROOT_SIGNATURE_DESC desc;
    desc.Init(2, p, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> sigBlob = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, sigBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);
    _device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&_rootSignature));
}

void CsVecLen::BuildShaderAndInputLayout()
{
    _shaders["vecLen"] = D3DUtil::CompileShader(L"Shaders\\VecLength.hlsl", nullptr, "main", "cs_5_1");
}

void CsVecLen::BuildPSOs()
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso;
    ZeroMemory(&pso, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));
    pso.CS = { _shaders["vecLen"]->GetBufferPointer(), (UINT)_shaders["vecLen"]->GetBufferSize() };
    pso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    pso.pRootSignature = _rootSignature.Get();
    ThrowIfFailed(_device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&_PSOs["vecLen"])));
}
