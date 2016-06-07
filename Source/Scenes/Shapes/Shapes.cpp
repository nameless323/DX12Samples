#include "Shapes.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

Shapes::Shapes(HINSTANCE hInstance) : Application(hInstance)
{
}

bool Shapes::Init()
{
    if (!Application::Init())
        return false;
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));
    BuildRootSignature();
    BuildShaderAndInputLayout();
    BuildShapeGeometry();
    BuildRenderItems();
    BuildFrameResources();
    BuildDescriptorHeaps();
    BuildConstantBufferViews();
    BuildPSOs();


}

Shapes::~Shapes()
{
}

LRESULT Shapes::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int Shapes::Run()
{
    return Application::Run();
}

void Shapes::OnResize()
{
    Application::OnResize();

    XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&_proj, p);
}

void Shapes::Update(const GameTimer& timer)
{
    OnKeyboardInput(timer);
    UpdateCamera(timer);

    _currentFrameResourceIndex = (_currentFrameResourceIndex + 1) % NumFrameResources;
    _currFrameResource = _frameResources[_currentFrameResourceIndex].get();

    if (_currFrameResource->Fence != 0 && _fence->GetCompletedValue() < _currFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(_fence->SetEventOnCompletion(_currFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
    UpdateObjectCBs(timer);
    UpdateMainPassCB(timer);
}

void Shapes::Draw(const GameTimer& timer)
{
    auto cmdListAlloc = _currFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());

    if (_isWireframe)
    {
        ThrowIfFailed(_commandList->Reset(cmdListAlloc.Get(), _PSOs["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(_commandList->Reset(cmdListAlloc.Get(), _PSOs["opaque"].Get()));
    }
    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    _commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    _commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    _commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { _cbvHeap.Get() };
    _commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    _commandList->SetGraphicsRootSignature(_rootSignature.Get());

    int passCbvIndex = _passCbvOffset + _currentFrameResourceIndex;
    auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(_cbvHeap->GetGPUDescriptorHandleForHeapStart());
    passCbvHandle.Offset(passCbvIndex, _cbvSrvUavDescriptorSize);
    _commandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

    DrawRenderItems(_commandList.Get(), _opaqueRenderItems);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(1, cmdLists);

    ThrowIfFailed(_swapChain->Present(0, 0));
    _currBackBuffer = (_currBackBuffer + 1) % _swapChainBufferCount;
    _currFrameResource->Fence = ++_currentFence;
    _commandQueue->Signal(_fence.Get(), _currentFence);
}

void Shapes::OnMouseDown(WPARAM btnState, int x, int y)
{
    _lastMousePos.x = x;
    _lastMousePos.y = y;
    SetCapture(_hMainWindow);
}

void Shapes::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void Shapes::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - _lastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(x - _lastMousePos.y));

        _theta += dx;
        _phi += dy;

        _phi = MathHelper::Clamp(_phi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = 0.05f * static_cast<float>(x - _lastMousePos.x);
        float dy = 0.05f * static_cast<float>(y - _lastMousePos.y);

        _radius += dx - dy;

        _radius = MathHelper::Clamp(_radius, 5.0f, 150.0f);
    }
    _lastMousePos.x = x;
    _lastMousePos.y = y;
}

void Shapes::OnKeyboardInput(const GameTimer& timer)
{
    if (GetAsyncKeyState('1') & 0x8000)
        _isWireframe = true;
    else
        _isWireframe = false;
}

void Shapes::UpdateCamera(const GameTimer& timer)
{
    _eyePos.x = _radius*sinf(_phi)*cosf(_theta);
    _eyePos.z = _radius*sinf(_phi)*cosf(_theta);
    _eyePos.y = _radius*cosf(_phi);

    XMVECTOR pos = XMVectorSet(_eyePos.x, _eyePos.y, _eyePos.z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&_view, view);
}

void Shapes::UpdateObjectCBs(const GameTimer& timer)
{
    auto currObjectCB = _currFrameResource->ObjectCB.get();
    for (auto& e : _allRenderItems)
    {
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX model = XMLoadFloat4x4(&e->Model);
            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.Model, XMMatrixTranspose(model));
            currObjectCB->CopyData(e->ObjCBIndex, objConstants);
            e->NumFramesDirty--;
        }
    }
}

void Shapes::UpdateMainPassCB(const GameTimer& timer)
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

    auto currPassCB = _currFrameResource->PassCB.get();
    currPassCB->CopyData(0, _mainPassCB);
}

void Shapes::BuildDescriptorHeaps()
{
}

void Shapes::BuildConstantBufferViews()
{
}

void Shapes::BuildRootSignature()
{
}

void Shapes::BuildShaderAndInputLayout()
{
}

void Shapes::BuildShapeGeometry()
{
}

void Shapes::BuildPSOs()
{
}

void Shapes::BuildFrameResources()
{
}

void Shapes::BuildRenderItems()
{
}

void Shapes::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<ShapesRenderItem*>& renderItems)
{
}
