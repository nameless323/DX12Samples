#include "Ch6Ex.h"

using namespace DirectX;
using namespace PackedVector;
using Microsoft::WRL::ComPtr;

Ch6Ex::Ch6Ex(HINSTANCE hInstance) : Application(hInstance)
{
}

Ch6Ex::~Ch6Ex()
{
}


bool Ch6Ex::Init()
{
    if (!Application::Init())
        return false;
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildGeometry();
    BuildPSO();

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdLists[] = {_commandList.Get()};
    _commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    FlushCommandQueue();
    return true;
}


LRESULT Ch6Ex::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int Ch6Ex::Run()
{
    return Application::Run();
}

void Ch6Ex::OnResize()
{
    Application::OnResize();
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&_proj, P);
}

void Ch6Ex::Update(const GameTimer& timer)
{
    //box
    float x = _radius * sinf(_phi) * cosf(_theta);
    float z = _radius * sinf(_phi) * sinf(_theta);
    float y = _radius * cosf(_phi);

    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&_view, view);

    XMMATRIX model = XMLoadFloat4x4(&_model)  * XMMatrixTranslation(-2, 0, 0) * XMMatrixScaling(0.5f, 0.5f, 0.5f);
    XMMATRIX proj = XMLoadFloat4x4(&_proj);
    XMMATRIX mvp = model * view * proj;

    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.MVP, XMMatrixTranspose(mvp));
    _objectCB->CopyData(0, objConstants);

    //pyramide
    x = _radius * sinf(_phi) * cosf(_theta);
    z = _radius * sinf(_phi) * sinf(_theta);
    y = _radius * cosf(_phi);

    pos = XMVectorSet(x, y, z, 1.0f);
    target = XMVectorZero();
    up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&_view, view);

    model = XMLoadFloat4x4(&_model) * XMMatrixTranslation(2, 0, 0) * XMMatrixScaling(0.5f, 0.5f, 0.5f);
    proj = XMLoadFloat4x4(&_proj);
    mvp = model * view * proj;

    ObjectConstants objConstantsPira;
    XMStoreFloat4x4(&objConstantsPira.MVP, XMMatrixTranspose(mvp));

    _objectCB->CopyData(1, objConstantsPira);

    //_timeCB->CopyData(0, timer.TotalTime());
    _timeCB->CopyData(0, timer.TotalTime());
    _timeCB->CopyData(1, timer.TotalTime());
}

void Ch6Ex::Draw(const GameTimer& timer)
{
    ThrowIfFailed(_commandAllocator->Reset());
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), _PSO.Get()));

    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    _commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    _commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    _commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = {_cbvHeap.Get()};
    _commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    _commandList->SetGraphicsRootSignature(_rootSignature.Get());
    _commandList->IASetVertexBuffers(0, 1, &_geometry->VertexBufferView());
    _commandList->IASetVertexBuffers(1, 1, &_geometry->VertexBufferViewSlot2());
    _commandList->IASetIndexBuffer(&_geometry->IndexBufferView());
    _commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    CD3DX12_GPU_DESCRIPTOR_HANDLE heapHandle(_cbvHeap->GetGPUDescriptorHandleForHeapStart());
    _commandList->SetGraphicsRootDescriptorTable(0, heapHandle);
    heapHandle.Offset(2, _cbvSrvUavDescriptorSize);
    _commandList->SetGraphicsRootDescriptorTable(1, heapHandle);

    _commandList->DrawIndexedInstanced(_geometry->DrawArgs["box"].IndexCount, 1, _geometry->DrawArgs["box"].StartIndexLocation, _geometry->DrawArgs["box"].BaseVertexLocation, 0);

    heapHandle = _cbvHeap->GetGPUDescriptorHandleForHeapStart();
    heapHandle.Offset(1, _cbvSrvUavDescriptorSize);
    _commandList->SetGraphicsRootDescriptorTable(0, heapHandle);
    _commandList->DrawIndexedInstanced(_geometry->DrawArgs["pyramide"].IndexCount, 1, _geometry->DrawArgs["pyramide"].StartIndexLocation, _geometry->DrawArgs["pyramide"].BaseVertexLocation, 0);
    
    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* cmdLists[] = {_commandList.Get()};
    _commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    ThrowIfFailed(_swapChain->Present(0, 0));
    _currBackBuffer = (_currBackBuffer + 1) % _swapChainBufferCount;

    FlushCommandQueue();
}

void Ch6Ex::OnMouseDown(WPARAM btnState, int x, int y)
{
    _lastMousePos.x = x;
    _lastMousePos.y = y;
    SetCapture(_hMainWindow);
}

void Ch6Ex::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void Ch6Ex::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - _lastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - _lastMousePos.y));

        _theta += dx;
        _phi += dy;

        _phi = MathHelper::Clamp(_phi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = 0.005f * static_cast<float>(x - _lastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - _lastMousePos.y);

        _radius += dx - dy;
        _radius = MathHelper::Clamp(_radius, 3.0f, 15.0f);
    }
    _lastMousePos.x = x;
    _lastMousePos.y = y;
}

void Ch6Ex::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 4;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&_cbvHeap)));
}

void Ch6Ex::BuildConstantBuffers()
{
    _objectCB = std::make_unique<UploadBuffer<ObjectConstants>>(_device.Get(), 2, true);
    UINT constantsBufferSize = D3DUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT objCBByteSize = constantsBufferSize;
    D3D12_GPU_VIRTUAL_ADDRESS cbAdderss = _objectCB->Resource()->GetGPUVirtualAddress();
    int boxCBufIndex = 0;
    cbAdderss += boxCBufIndex * objCBByteSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAdderss;
    cbvDesc.SizeInBytes = constantsBufferSize;

    _device->CreateConstantBufferView(&cbvDesc, _cbvHeap->GetCPUDescriptorHandleForHeapStart());

    int pyroCBufIndex = 1;
    objCBByteSize = constantsBufferSize;
    cbAdderss = _objectCB->Resource()->GetGPUVirtualAddress();
    cbAdderss += pyroCBufIndex * objCBByteSize;
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDescPyro;
    cbvDescPyro.BufferLocation = cbAdderss;
    cbvDescPyro.SizeInBytes = constantsBufferSize;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHeapHandle(_cbvHeap->GetCPUDescriptorHandleForHeapStart());
    cbvHeapHandle.Offset(1, _cbvSrvUavDescriptorSize); 

    _device->CreateConstantBufferView(&cbvDescPyro, cbvHeapHandle);


    _timeCB = std::make_unique<UploadBuffer<float>>(_device.Get(), 1, true);
    objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(float));
    cbAdderss = _timeCB->Resource()->GetGPUVirtualAddress();
    D3D12_CONSTANT_BUFFER_VIEW_DESC timeDesc;
    timeDesc.BufferLocation = cbAdderss;
    timeDesc.SizeInBytes = objCBByteSize;

    cbvHeapHandle.Offset(1, _cbvSrvUavDescriptorSize);
    _device->CreateConstantBufferView(&timeDesc, cbvHeapHandle);
}

void Ch6Ex::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];

    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE cbvTableTime;
    cbvTableTime.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);
    slotRootParameter[1].InitAsDescriptorTable(1, &cbvTableTime);


    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&_rootSignature)));
}

void Ch6Ex::BuildShadersAndInputLayout()
{
    _vsByteCode = D3DUtil::CompileShader(L"Shaders\\Ch6Ex.hlsl", nullptr, "vert", "vs_5_1");
    _psByteCode = D3DUtil::CompileShader(L"Shaders\\Ch6Ex.hlsl", nullptr, "frag", "ps_5_1");
    _inputLayout =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        };
}

void Ch6Ex::BuildGeometry()
{
    std::array<VertexPosData, 13> positions =
        {
            VertexPosData({XMFLOAT3(-1.0f, -1.0f, -1.0f)}),
            VertexPosData({XMFLOAT3(-1.0f, +1.0f, -1.0f)}),
            VertexPosData({XMFLOAT3(+1.0f, +1.0f, -1.0f)}),
            VertexPosData({XMFLOAT3(+1.0f, -1.0f, -1.0f)}),
            VertexPosData({XMFLOAT3(-1.0f, -1.0f, +1.0f)}),
            VertexPosData({XMFLOAT3(-1.0f, +1.0f, +1.0f)}),
            VertexPosData({XMFLOAT3(+1.0f, +1.0f, +1.0f)}),
            VertexPosData({XMFLOAT3(+1.0f, -1.0f, +1.0f)}),

            //pyramide
            VertexPosData({ XMFLOAT3(0.0f, 0.7f, 0.0f) }),
            VertexPosData({ XMFLOAT3(-1.0f, -0.7f, -1.0f) }),
            VertexPosData({ XMFLOAT3(-1.0f, -0.7f, 1.0f) }),
            VertexPosData({ XMFLOAT3(1.0f, -0.7f, 1.0f) }),
            VertexPosData({ XMFLOAT3(1.0f, -0.7f, -1.0f) })
        };

    std::array<VertexColorData, 13> colors =
        {
            VertexColorData({XMFLOAT4(Colors::White)}),
            VertexColorData({XMFLOAT4(Colors::Black)}),
            VertexColorData({XMFLOAT4(Colors::Red)}),
            VertexColorData({XMFLOAT4(Colors::Green)}),
            VertexColorData({XMFLOAT4(Colors::Blue)}),
            VertexColorData({XMFLOAT4(Colors::Yellow)}),
            VertexColorData({XMFLOAT4(Colors::Cyan)}),
            VertexColorData({XMFLOAT4(Colors::Magenta)}),

            //pyramide
            VertexColorData({ XMFLOAT4(Colors::AliceBlue) }),
            VertexColorData({ XMFLOAT4(Colors::Gainsboro) }),
            VertexColorData({ XMFLOAT4(Colors::SteelBlue) }),
            VertexColorData({ XMFLOAT4(Colors::DarkOrange) }),
            VertexColorData({ XMFLOAT4(Colors::DarkGoldenrod) })
        };


    std::array<std::uint16_t, 54> indices =
        {
            // front face
            0, 1, 2,
            0, 2, 3,

            // back face
            4, 6, 5,
            4, 7, 6,

            // left face
            4, 5, 1,
            4, 1, 0,

            // right face
            3, 2, 6,
            3, 6, 7,

            // top face
            1, 5, 6,
            1, 6, 2,

            // bottom face
            4, 0, 3,
            4, 3, 7,

            //sides
            0, 1, 2,
            0, 2, 3,
            0, 3, 4,
            0, 4, 1,

            //bottom
            1, 3, 2,
            3, 1, 4
        };

    const UINT vbPosByteSize = (UINT)positions.size() * sizeof(VertexPosData);
    const UINT vbColByteSize = (UINT)colors.size() * sizeof(VertexColorData);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    _geometry = std::make_unique<MeshGeometry>();
    _geometry->Name = "boxGeo";

    _geometry->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), positions.data(), vbPosByteSize, _geometry->VertexBufferUploader);
    _geometry->VertexBufferGPUSlot2 = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), colors.data(), vbColByteSize, _geometry->VertexBufferUploaderSlot2);
    _geometry->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), indices.data(), ibByteSize, _geometry->IndexBufferUploader);

    _geometry->VertexByteStride = sizeof(VertexPosData);
    _geometry->VertexBufferByteSize = vbPosByteSize;
    _geometry->VertexByteStrideSlot2 = sizeof(VertexColorData);
    _geometry->VertexBufferByteSizeSlot2 = vbColByteSize;

    _geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
    _geometry->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry boxGeometry;
    boxGeometry.IndexCount = 36;
    boxGeometry.StartIndexLocation = 0;
    boxGeometry.BaseVertexLocation = 0;
    _geometry->DrawArgs["box"] = boxGeometry;

    SubmeshGeometry pyramideGeometry;
    pyramideGeometry.IndexCount = 18;
    pyramideGeometry.StartIndexLocation = 36;
    pyramideGeometry.BaseVertexLocation = 8;
    _geometry->DrawArgs["pyramide"] = pyramideGeometry;
}


void Ch6Ex::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = {_inputLayout.data(), (UINT)_inputLayout.size()};
    psoDesc.pRootSignature = _rootSignature.Get();

    psoDesc.VS =
        {
            reinterpret_cast<BYTE*>(_vsByteCode->GetBufferPointer()), _vsByteCode->GetBufferSize()
        };
    psoDesc.PS =
        {
            reinterpret_cast<BYTE*>(_psByteCode->GetBufferPointer()), _psByteCode->GetBufferSize()
        };

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    //psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = _backBufferFormat;
    psoDesc.SampleDesc.Count = _4xMsaa ? 4 : 1;
    psoDesc.SampleDesc.Quality = _4xMsaa ? (_4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = _dsvFormat;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_PSO)));
}
