#include "CSVectorAdd.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;

CSVectorAdd::CSVectorAdd(HINSTANCE hInstance) : Application(hInstance)
{
}

CSVectorAdd::~CSVectorAdd()
{
    if (_device != nullptr)
        FlushCommandQueue();
}

bool CSVectorAdd::Init()
{
    if (!Application::Init())
        return false;

    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    BuildBuffers();
    BuildRootSignature();
    BuildShaderAndInputLayout();
    BuildPSOs();

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();
    DoComputeWork();

    return true;
}

LRESULT CSVectorAdd::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int CSVectorAdd::Run()
{
    return Application::Run();
}

void CSVectorAdd::OnResize()
{
}

void CSVectorAdd::Update(const GameTimer& timer)
{    
}

void CSVectorAdd::Draw(const GameTimer& timer)
{
    FlushCommandQueue();
}

void CSVectorAdd::DoComputeWork()
{
    ThrowIfFailed(_commandAllocator->Reset());
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), _PSOs["vecAdd"].Get()));

    _commandList->SetComputeRootSignature(_rootSignature.Get());

    _commandList->SetComputeRootShaderResourceView(0, _inputBufferA->GetGPUVirtualAddress());
    _commandList->SetComputeRootShaderResourceView(1, _inputBufferB->GetGPUVirtualAddress());
    _commandList->SetComputeRootUnorderedAccessView(2, _outputBuffer->GetGPUVirtualAddress());

    _commandList->Dispatch(1, 1, 1);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_outputBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE));

    _commandList->CopyResource(_readBackBuffer.Get(), _outputBuffer.Get());
    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_outputBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON));

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
    FlushCommandQueue();

    Data* mappedData = nullptr;
    _readBackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));

    std::ofstream fout("results.txt");
    for (int i = 0; i < NumDataElements; i++)
    {
        fout << "(" << mappedData[i].v1.x << ", " << mappedData[i].v1.y << ", " << mappedData[i].v1.z <<
            ", " << mappedData[i].v2.x << ", " << mappedData[i].v2.y << ")" << std::endl;
    }
    _readBackBuffer->Unmap(0, nullptr);
}

void CSVectorAdd::BuildBuffers()
{
    std::vector<Data> dataA(NumDataElements);
    std::vector<Data> dataB(NumDataElements);

    for (int i = 0; i < NumDataElements; i++)
    {
        dataA[i].v1 = XMFLOAT3(i, i, i);
        dataA[i].v2 = XMFLOAT2(i, 0);

        dataB[i].v1 = XMFLOAT3(-i, i, 0.0f);
        dataB[i].v2 = XMFLOAT2(0, -i);
    }
    UINT64 byteSize = dataA.size() * sizeof(Data);

    _inputBufferA = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), dataA.data(), byteSize, _inputUploadBufferA);
    _inputBufferB = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), dataB.data(), byteSize, _inputUploadBufferB);
        
    ThrowIfFailed(_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(byteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&_outputBuffer)));
    ThrowIfFailed(_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(byteSize), D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_readBackBuffer)));
}

void CSVectorAdd::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[3];

    slotRootParameter[0].InitAsShaderResourceView(0);
    slotRootParameter[1].InitAsShaderResourceView(1);
    slotRootParameter[2].InitAsUnorderedAccessView(0);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
    if (errorBlob != nullptr)
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(_rootSignature.GetAddressOf())));

}

void CSVectorAdd::BuildShaderAndInputLayout()
{
    _shaders["vecAddCS"] = D3DUtil::CompileShader(L"Shaders\\VecAdd.hlsl", nullptr, "compAdd", "cs_5_0");
}

void CSVectorAdd::BuildPSOs()
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
    computePsoDesc.pRootSignature = _rootSignature.Get();
    computePsoDesc.CS =
    {
        _shaders["vecAddCS"]->GetBufferPointer(),
        _shaders["vecAddCS"]->GetBufferSize()
    };
    computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    _device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&_PSOs["vecAdd"]));
}
