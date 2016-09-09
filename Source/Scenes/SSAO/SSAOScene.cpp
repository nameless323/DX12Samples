#include "SSAOScene.h"

#include "../../../Core/GeometryGenerator.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;
using Vertex = SSAOFrameResource::Vertex;
using FrameResource = SSAOFrameResource;
using RenderLayer = RenderItem::RenderLayer;

SSAOScene::SSAOScene(HINSTANCE hInstance) : Application(hInstance)
{
    _sceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    _sceneBounds.Radius = sqrtf(10.0f * 10.0f + 15.0f * 15.0f);
}

bool SSAOScene::Init()
{
    if (!Application::Init())
        return false;
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    _camera.SetPosition(0.0f, 2.0f, -15.0f);

    _shadowMap = std::make_unique<ShadowMap>(_device.Get(), 2048, 2048);
    _ssao = std::make_unique<SSAO>(_device.Get(), _commandList.Get(), _clientWidth, _clientHeight);

    LoadTextures();
    BuildRootSignature();
    BuildSSAORootSignature();
    BuildDescriptorHeaps();
    BuildShaderAndInputLayout();
    BuildShapeGeometry();
    BuildSkullGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();
    
    _ssao->SetPSOs(_PSOs["ssao"].Get(), _PSOs["ssaoBlur"].Get());

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();

    return true;
}

SSAOScene::~SSAOScene()
{
    if (_device != nullptr)
        FlushCommandQueue();    
}

LRESULT SSAOScene::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int SSAOScene::Run()
{
    return Application::Run();
}

void SSAOScene::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = _swapChainBufferCount + 3;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(_rtvHeap.GetAddressOf())));
    
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 2;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.GetAddressOf())));
}

void SSAOScene::OnResize()
{
    Application::OnResize();
    _camera.SetFrustum(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    if (_ssao != nullptr)
    {
        _ssao->OnResize(_clientWidth, _clientHeight);
        _ssao->RebuildDescriptors(_depthStencilBuffer.Get());
    }
}

void SSAOScene::Update(const GameTimer& timer)
{
    OnKeyboardInput(timer);

    _currentFrameResourceIndex = (_currentFrameResourceIndex + 1) % SSAOFrameResource::NumFrameResources;
    _currFrameResource = _frameResources[_currentFrameResourceIndex].get();

    if (_currFrameResource->Fence != 0 && _fence->GetCompletedValue() < _currFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(_fence->SetEventOnCompletion(_currFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
    
    _lightRotationAngle += 0.1f * _timer.DeltaTime();
    XMMATRIX R = XMMatrixRotationY(_lightRotationAngle);
    for (int i = 0; i < 3; i++)
    {
        XMVECTOR lightDir = XMLoadFloat3(&_baseLightDirections[i]);
        lightDir = XMVector3TransformNormal(lightDir, R);
        XMStoreFloat3(&_rotatedLightDirections[i], lightDir);
    }
    AnimateMaterials(timer);
    UpdateObjectCBs(timer);
    UpdateMaterialBuffer(timer);
    UpdateShadowTransform(timer);
    UpdateMainPassCB(timer);
    UpdateShadowPassCB(timer);
    UpdateSSAOCB(timer);
}

void SSAOScene::Draw(const GameTimer& timer)
{
    auto cmdListAlloc = _currFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(_commandList->Reset(cmdListAlloc.Get(), _PSOs["opaque"].Get()));

    ID3D12DescriptorHeap* descriptorHeaps[] = { _srvHeap.Get() };
    _commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    _commandList->SetGraphicsRootSignature(_rootSignature.Get());

    auto matBuffer = _currFrameResource->MaterialBuffer->Resource();
    _commandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());
    _commandList->SetGraphicsRootDescriptorTable(3, _nullSrv);

    _commandList->SetGraphicsRootDescriptorTable(4, _srvHeap->GetGPUDescriptorHandleForHeapStart());

    DrawSceneToShadowMap();

    DrawNormalsAndDepth();

    _commandList->SetGraphicsRootSignature(_ssaoRootSignature.Get());
    _ssao->ComputeSSAO(_commandList.Get(), _currFrameResource, 3);

    _commandList->SetGraphicsRootSignature(_rootSignature.Get());

    matBuffer = _currFrameResource->MaterialBuffer->Resource();
    _commandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());

    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    _commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    _commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    _commandList->SetGraphicsRootDescriptorTable(4, _srvHeap->GetGPUDescriptorHandleForHeapStart());
    
    auto passCB = _currFrameResource->PassCB->Resource();
    _commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

    CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(_srvHeap->GetGPUDescriptorHandleForHeapStart());
    skyTexDescriptor.Offset(_skyTexHeapIndex, _cbvSrvUavDescriptorSize);
    _commandList->SetGraphicsRootDescriptorTable(3, skyTexDescriptor);

    _commandList->SetPipelineState(_PSOs["opaque"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::Opaque]);

    _commandList->SetPipelineState(_PSOs["debug"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::Debug]);

    _commandList->SetPipelineState(_PSOs["sky"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::Sky]);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    ThrowIfFailed(_swapChain->Present(0, 0));
    _currBackBuffer = (_currBackBuffer + 1) % _swapChainBufferCount;

    _currFrameResource->Fence = ++_currentFence;

    _commandQueue->Signal(_fence.Get(), _currentFence);
}

void SSAOScene::OnMouseDown(WPARAM btnState, int x, int y)
{
}

void SSAOScene::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void SSAOScene::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void SSAOScene::OnKeyboardInput(const GameTimer& timer)
{
}

void SSAOScene::AnimateMaterials(const GameTimer& timer)
{
}

void SSAOScene::UpdateObjectCBs(const GameTimer& timer)
{
}

void SSAOScene::UpdateMaterialBuffer(const GameTimer& timer)
{
}

void SSAOScene::UpdateShadowTransform(const GameTimer& timer)
{
}

void SSAOScene::UpdateMainPassCB(const GameTimer& timer)
{
}

void SSAOScene::UpdateShadowPassCB(const GameTimer& timer)
{
}

void SSAOScene::UpdateSSAOCB(const GameTimer& timer)
{
}

void SSAOScene::LoadTextures()
{
}

void SSAOScene::BuildRootSignature()
{
}

void SSAOScene::BuildSSAORootSignature()
{
}

void SSAOScene::BuildDescriptorHeaps()
{
}

void SSAOScene::BuildShaderAndInputLayout()
{
}

void SSAOScene::BuildShapeGeometry()
{
}

void SSAOScene::BuildSkullGeometry()
{
}

void SSAOScene::BuildPSOs()
{
}

void SSAOScene::BuildFrameResources()
{
}

void SSAOScene::BuildMaterials()
{
}

void SSAOScene::BuildRenderItems()
{
}

void SSAOScene::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems)
{
}

void SSAOScene::DrawSceneToShadowMap()
{
}

void SSAOScene::DrawNormalsAndDepth()
{
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SSAOScene::GetCpuSrv(int index) const
{
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SSAOScene::GetGpuSrv(int index) const
{
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SSAOScene::GetDsv(int index) const
{
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SSAOScene::GetRtv(int index) const
{
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> SSAOScene::GetStaticSamplers()
{
    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC shadow(
        6, // shaderRegister
        D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
        0.0f,                               // mipLODBias
        16,                                 // maxAnisotropy
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

    return{
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp,
        shadow
    };
}
