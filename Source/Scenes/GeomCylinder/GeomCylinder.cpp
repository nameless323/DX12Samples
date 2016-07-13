#include "GeomCylinder.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace Colors;
using namespace PackedVector;

GeomCylinder::GeomCylinder(HINSTANCE hInstance) : Application(hInstance)
{
}

bool GeomCylinder::Init()
{
    if (!Application::Init())
        return false;
    ThrowIfFailed(_commandAllocator->Reset());
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    BuildShapeGeometry();
    BuildShaderAndInputLayout();
    BuildRootSignature();
    BuildRenderItems();
    BuildFrameResources();

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
    FlushCommandQueue();

    return true;
}

GeomCylinder::~GeomCylinder()
{
    if (_device != nullptr)
        FlushCommandQueue();
}

LRESULT GeomCylinder::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int GeomCylinder::Run()
{
    return Application::Run();
}

void GeomCylinder::OnResize()
{
    Application::OnResize();
    XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 100.0f);
    XMStoreFloat4x4(&_proj, XMMatrixTranspose(p));
}

void GeomCylinder::Update(const GameTimer& timer)
{
    UpdateCamera(timer);

    _currentFrameResourceIndex = (_currentFrameResourceIndex + 1) % CrateFrameResource::NumFrameResources;
    _currFrameResource = _frameResources[_currentFrameResourceIndex].get();

    if (_currFrameResource->Fence != 0 && _currFrameResource->Fence > _fence->GetCompletedValue())
    {
        HANDLE e;
        e = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        ThrowIfFailed(_fence->SetEventOnCompletion(_currFrameResource->Fence, e));

        WaitForSingleObject(e, INFINITE);
        CloseHandle(e);
    }

    UpdateMainPassCB(timer);
    UpdateObjectCBs(timer);
}

void GeomCylinder::Draw(const GameTimer& timer)
{

}

void GeomCylinder::OnMouseDown(WPARAM btnState, int x, int y)
{
    _lastMousePos.x = x;
    _lastMousePos.y = y;
    SetCapture(_hMainWindow);
}

void GeomCylinder::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void GeomCylinder::OnMouseMove(WPARAM btnState, int x, int y)
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
        float dx = 0.01f * static_cast<float>(x - _lastMousePos.x);
        float dy = 0.01f * static_cast<float>(y - _lastMousePos.y);

        _radius += dx - dy;

        _radius = MathHelper::Clamp(_radius, 1.0f, 150.0f);
    }
    _lastMousePos.x = x;
    _lastMousePos.y = y;
}

void GeomCylinder::OnKeyboardInput(const GameTimer& timer)
{
}

void GeomCylinder::UpdateCamera(const GameTimer& timer)
{
    _eyePos.x = _radius * sinf(_phi) * cosf(_theta);
    _eyePos.z = _radius * sinf(_phi) * sinf(_theta);
    _eyePos.y = _radius * cosf(_phi);

    XMVECTOR pos = XMVectorSet(_eyePos.x, _eyePos.y, _eyePos.z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&_view, view);
}

void GeomCylinder::UpdateMainPassCB(const GameTimer& timer)
{
    XMMATRIX view = XMLoadFloat4x4(&_view);
    XMMATRIX proj = XMLoadFloat4x4(&_proj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&_mainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&_mainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&_mainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&_mainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&_mainPassCB.VP, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&_mainPassCB.InvVP, XMMatrixTranspose(invViewProj));
    _mainPassCB.EyePosW = _eyePos;
    _mainPassCB.RenderTargetSize = XMFLOAT2((float)_clientWidth, (float)_clientHeight);
    _mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / _clientWidth, 1.0f / _clientHeight);
    _mainPassCB.NearZ = 1.0f;
    _mainPassCB.FarZ = 1000.0f;
    _mainPassCB.TotalTime = timer.TotalTime();
    _mainPassCB.DeltaTime = timer.DeltaTime();
    _mainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

    _mainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    _mainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
    _mainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    _mainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
    _mainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    _mainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

    auto currPassCB = _currFrameResource->PassCB.get();
    currPassCB->CopyData(0, _mainPassCB);
}

void GeomCylinder::UpdateObjectCBs(const GameTimer& timer)
{
    for (auto& e : _allRenderItems)
    {
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX model = XMMatrixTranspose(XMLoadFloat4x4(&e->Model));
            XMMATRIX texTransform = XMMatrixTranspose(XMLoadFloat4x4(&e->TexTransform));

            CrateFrameResource::ObjectConstants o;
            XMStoreFloat4x4(&e->Model, model);
            XMStoreFloat4x4(&e->TexTransform, texTransform);

            _currFrameResource->ObjectCB->CopyData(e->ObjCBIndex, o);

            e->NumFramesDirty--;
        }
    }
}

void GeomCylinder::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slots[2];
    slots[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
    slots[1].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC signature;
    signature.Init(2, slots);
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    ComPtr<ID3DBlob> serializedSignature = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&signature, D3D_ROOT_SIGNATURE_VERSION_1, serializedSignature.GetAddressOf(), errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
    {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);
    _device->CreateRootSignature(0, serializedSignature->GetBufferPointer(), serializedSignature->GetBufferSize(), IID_PPV_ARGS(_rootSignature.GetAddressOf()));
}

void GeomCylinder::BuildShaderAndInputLayout()
{
    _shaders["vs"] = D3DUtil::CompileShader(L"Shaders\\GeomCylinder.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["gs"] = D3DUtil::CompileShader(L"Shaders\\GeomCylinder.hlsl", nullptr, "geom", "gs_5_1");
    _shaders["ps"] = D3DUtil::CompileShader(L"Shaders\\GeomCylinder.hlsl", nullptr, "frag", "fs_5_1");

    D3D12_INPUT_ELEMENT_DESC element;
    element.SemanticName = "POSITION";
    element.SemanticIndex = 0;
    element.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    element.InputSlot = 0;
    element.AlignedByteOffset = 0;
    element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    element.InstanceDataStepRate = 0;
    _inputLayout =
    {
        element
    };
}

void GeomCylinder::BuildShapeGeometry()
{
}

void GeomCylinder::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    psoDesc.InputLayout = { _inputLayout.data(), (UINT)_inputLayout.size() };
    psoDesc.pRootSignature = _rootSignature.Get();

    psoDesc.VS = { reinterpret_cast<BYTE*>(_shaders["vert"]->GetBufferPointer()), _shaders["vert"]->GetBufferSize() };
    psoDesc.GS = { reinterpret_cast<BYTE*>(_shaders["geom"]->GetBufferPointer()), _shaders["geom"]->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(_shaders["frag"]->GetBufferPointer()), _shaders["frag"]->GetBufferSize() };

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    psoDesc.DSVFormat = _dsvFormat;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = _backBufferFormat;
    psoDesc.NodeMask = 0;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.SampleDesc.Count = _4xMsaa ? 4 : 1;
    psoDesc.SampleDesc.Quality = _4xMsaa ? _4xMsaaQuality - 1 : 1;
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(_opaquePSO.GetAddressOf())));    
}

void GeomCylinder::BuildFrameResources()
{
    for (int i = 0; i < CrateFrameResource::NumFrameResources; i++)
        _frameResources.push_back(std::make_unique<CrateFrameResource>(_device.Get(), 1, _allRenderItems.size(), 0));
}

void GeomCylinder::BuildRenderItems()
{
    auto renderItem = std::make_unique<CrateRenderItem>();
    renderItem->ObjCBIndex = 0;
    renderItem->Model = MathHelper::Identity4x4();
    renderItem->Geo = _geometries["circle"].get();
    renderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    renderItem->BaseVertexLocation = renderItem->Geo->DrawArgs["circle"].BaseVertexLocation;
    renderItem->StartIndexLocation = renderItem->Geo->DrawArgs["circle"].StartIndexLocation;
    renderItem->IndexCount = renderItem->Geo->DrawArgs["circle"].IndexCount;
    renderItem->NumFramesDirty = CrateFrameResource::NumFrameResources;

    _allRenderItems.push_back(move(renderItem));
    _opaqueRenderItems.push_back(renderItem.get());
}

void GeomCylinder::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<CrateRenderItem*>& renderItems)
{
}
