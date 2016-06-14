#include "WavesScene.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;

WavesScene::WavesScene(HINSTANCE hInstance) : Application(hInstance)
{
}

WavesScene::~WavesScene()
{
    if (_device != nullptr)
        FlushCommandQueue();
}

bool WavesScene::Init()
{
    if (!Application::Init())
        return false;
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    BuildRootSignature();
    BuildShaderAndInputLayout();
    BuildLangGeometry();
    BuildWavesGeometryBuffers();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(1, cmdLists);
    FlushCommandQueue();
    return true;
}

LRESULT WavesScene::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int WavesScene::Run()
{
    return Application::Run();
}

void WavesScene::OnResize()
{
    Application::OnResize();

    XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&_proj, p);
}

void WavesScene::Update(const GameTimer& timer)
{
}

void WavesScene::Draw(const GameTimer& timer)
{
}

void WavesScene::OnMouseDown(WPARAM btnState, int x, int y)
{
    _lastMousePos.x = x;
    _lastMousePos.y = y;
    SetCapture(_hMainWindow);
}

void WavesScene::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void WavesScene::OnMouseMove(WPARAM btnState, int x, int y)
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
        float dx = 0.2f * static_cast<float>(x - _lastMousePos.x);
        float dy = 0.2f * static_cast<float>(y - _lastMousePos.y);

        _radius += dx - dy;

        _radius = MathHelper::Clamp(_radius, 5.0f, 150.0f);
    }
    _lastMousePos.x = x;
    _lastMousePos.y = y;
}

void WavesScene::OnKeyboardInput(const GameTimer& timer)
{
    if (GetAsyncKeyState('1') & 0x8000)
        _isWireframe = true;
    else
        _isWireframe = false;
}

void WavesScene::UpdateCamera(const GameTimer& timer)
{
    _eyePos.x = _radius*sinf(_phi)*cosf(_theta);
    _eyePos.z = _radius*sinf(_phi)*sinf(_theta);
    _eyePos.y = _radius*cosf(_phi);

    XMVECTOR pos = XMVectorSet(_eyePos.x, _eyePos.y, _eyePos.z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&_view, view);
}

void WavesScene::UpdateObjectCBs(const GameTimer& timer)
{
}

void WavesScene::UpdateMainPassCB(const GameTimer& timer)
{
}

void WavesScene::UpdateWaves(const GameTimer& timer)
{
}

void WavesScene::BuildRootSignature()
{
}

void WavesScene::BuildShaderAndInputLayout()
{
}

void WavesScene::BuildLangGeometry()
{
}

void WavesScene::BuildWavesGeometryBuffers()
{
}

void WavesScene::BuildPSOs()
{
}

void WavesScene::BuildFrameResources()
{
}

void WavesScene::BuildRenderItems()
{
}

void WavesScene::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<WavesRenderItem*>& renderItems)
{
}

float WavesScene::GetHillsHeight(float x, float z) const
{
    return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 WavesScene::GetHillsNormal(float x, float z) const
{
    XMFLOAT3 n(-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z), 1.0f, -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));
    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
    XMStoreFloat3(&n, unitNormal);
    return n;
}
