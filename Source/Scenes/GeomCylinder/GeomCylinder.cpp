#include "GeomCylinder.h"

#include "../../../Core/GeometryGenerator.h"

namespace DX12Samples
{
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

    BuildRootSignature();
    BuildShaderAndInputLayout();
    BuildShapeGeometry();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

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
    XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&_proj, p);
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
    auto cmdAllocator = _currFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdAllocator->Reset());
    ThrowIfFailed(_commandList->Reset(cmdAllocator.Get(), _opaquePSO.Get()));

    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);

    _commandList->SetGraphicsRootSignature(_rootSignature.Get());
    _commandList->SetGraphicsRootConstantBufferView(1, _currFrameResource->PassCB->Resource()->GetGPUVirtualAddress());

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    _commandList->ResourceBarrier(1, &barrier);

    _commandList->ClearRenderTargetView(CurrentBackBufferView(), DarkBlue, 0, nullptr);
    _commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    _commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    DrawRenderItems(_commandList.Get(), _opaqueRenderItems);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    _commandList->ResourceBarrier(1, &barrier);
    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* cmdLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    _swapChain->Present(0, 0);
    _currBackBuffer = (_currBackBuffer + 1) % _swapChainBufferCount;
    _currFrameResource->Fence = ++_currentFence;
    ThrowIfFailed(_commandQueue->Signal(_fence.Get(), _currFrameResource->Fence));

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
    _shaders["ps"] = D3DUtil::CompileShader(L"Shaders\\GeomCylinder.hlsl", nullptr, "frag", "ps_5_1");

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
    const int step = 5;
    const int stepCount = 360 / step + 1;
    const float radius = 1;
    std::array<XMFLOAT3, stepCount> verts;
    std::array<int16_t, stepCount> indices;
    for (int i = 0; i < stepCount; i ++)
    {
        XMStoreFloat3(&verts[i], MathHelper::SphericalToCartesian(radius, i * step * MathHelper::Pi / 180.0f, MathHelper::Pi / 2.0f));
        indices[i] = i;
    }
    
    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "circle";

    size_t vetsByteSize = sizeof(XMFLOAT3) * stepCount;
    size_t indByteSize = sizeof(int16_t) * stepCount;

    geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), verts.data(), vetsByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), indices.data(), indByteSize, geo->IndexBufferUploader);

    SubmeshGeometry submesh;
    submesh.BaseVertexLocation = 0;
    submesh.StartIndexLocation = 0;
    submesh.IndexCount = stepCount;

    geo->IndexBufferByteSize = indByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->VertexByteStride = sizeof(XMFLOAT3);
    geo->VertexBufferByteSize = vetsByteSize;


    geo->DrawArgs["circle"] = submesh;
    _geometries[geo->Name] = std::move(geo);
}

void GeomCylinder::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;

    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    psoDesc.InputLayout = { _inputLayout.data(), (UINT)_inputLayout.size() };
    psoDesc.pRootSignature = _rootSignature.Get();

    psoDesc.VS = { /*reinterpret_cast<BYTE*>*/(_shaders["vs"]->GetBufferPointer()), _shaders["vs"]->GetBufferSize() };
    psoDesc.GS = { /*reinterpret_cast<BYTE*>*/(_shaders["gs"]->GetBufferPointer()), _shaders["gs"]->GetBufferSize() };
    psoDesc.PS = { /*reinterpret_cast<BYTE*>*/(_shaders["ps"]->GetBufferPointer()), _shaders["ps"]->GetBufferSize() };

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = _backBufferFormat;
    psoDesc.SampleDesc.Count = _4xMsaa ? 4 : 1;
    psoDesc.SampleDesc.Quality = _4xMsaa ? _4xMsaaQuality - 1 : 0;
    psoDesc.DSVFormat = _dsvFormat;
    psoDesc.NodeMask = 0;
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_opaquePSO)));    
}

void GeomCylinder::BuildFrameResources()
{
    for (int i = 0; i < CrateFrameResource::NumFrameResources; i++)
        _frameResources.push_back(std::make_unique<CrateFrameResource>(_device.Get(), 1, _allRenderItems.size(), _allRenderItems.size()));
}

void GeomCylinder::BuildRenderItems()
{
    auto renderItem = std::make_unique<CrateRenderItem>();
    renderItem->ObjCBIndex = 0;
    renderItem->Model = MathHelper::Identity4x4();
    renderItem->Geo = _geometries["circle"].get();
    renderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    renderItem->BaseVertexLocation = renderItem->Geo->DrawArgs["circle"].BaseVertexLocation;
    renderItem->StartIndexLocation = renderItem->Geo->DrawArgs["circle"].StartIndexLocation;
    renderItem->IndexCount = renderItem->Geo->DrawArgs["circle"].IndexCount;
    renderItem->NumFramesDirty = CrateFrameResource::NumFrameResources;

    _opaqueRenderItems.push_back(renderItem.get());
    _allRenderItems.push_back(move(renderItem));
}

void GeomCylinder::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<CrateRenderItem*>& renderItems)
{
    size_t objCbByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(CrateFrameResource::ObjectConstants));
    for (auto renderItem : renderItems)
    {
        cmdList->IASetVertexBuffers(0, 1, &renderItem->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&renderItem->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(renderItem->PrimitiveType);

        cmdList->SetGraphicsRootConstantBufferView(0, _currFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress() + renderItem->ObjCBIndex*objCbByteSize);
        cmdList->DrawInstanced(renderItem->IndexCount, 1, 0, 0);
    }
}
}