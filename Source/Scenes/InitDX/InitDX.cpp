#include "../../Core/Application.h"
#include "InitDX.h"
#include <DirectXColors.h>

using namespace DirectX;

InitDX::InitDX(HINSTANCE hInstance) : Application(hInstance)
{}

InitDX::~InitDX()
{}

LRESULT InitDX::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int InitDX::Run()
{
    return Application::Run();
}

void InitDX::CreateRtvAndDsvDescriptorHeaps()
{
    Application::CreateRtvAndDsvDescriptorHeaps();
}

bool InitDX::Init()
{
    if (!Application::Init())
        return false;
    return true;
}

void InitDX::OnResize()
{
    Application::OnResize();
}

void InitDX::Update(const GameTimer& timer)
{}

void InitDX::Draw(const GameTimer& timer)
{
    ThrowIfFailed(_commandAllocator->Reset());
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    _commandList->ResourceBarrier(1, &barrier);

    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);

    _commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::DarkSeaGreen, 0, nullptr);
    _commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0.0f, 0, nullptr);

    _commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* cmdsLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(_swapChain->Present(0, 0));
    _currBackBuffer = (_currBackBuffer + 1) % _swapChainBufferCount;

    FlushCommandQueue();
}

void InitDX::OnMouseDown(WPARAM btnState, int x, int y)
{
    Application::OnMouseDown(btnState, x, y);
}

void InitDX::OnMouseUp(WPARAM btnState, int x, int y)
{
    Application::OnMouseUp(btnState, x, y);
}

void InitDX::OnMouseMove(WPARAM btnState, int x, int y)
{
    Application::OnMouseMove(btnState, x, y);
}
