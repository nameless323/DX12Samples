#include "Shapes.h"


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
}

void Shapes::Update(const GameTimer& timer)
{
}

void Shapes::Draw(const GameTimer& timer)
{
}

void Shapes::OnMouseDown(WPARAM btnState, int x, int y)
{
}

void Shapes::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void Shapes::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void Shapes::OnKeyboardInput(const GameTimer& timer)
{
}

void Shapes::UpdateCamera(const GameTimer& timer)
{
}

void Shapes::UpdateObjectCBs(const GameTimer& timer)
{
}

void Shapes::UpdateMainPassCB(const GameTimer& timer)
{
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
