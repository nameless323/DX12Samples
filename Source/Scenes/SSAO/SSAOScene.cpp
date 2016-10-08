#include "SSAOScene.h"

#include "../../../Core/GeometryGenerator.h"

namespace DX12Samples
{
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


    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_ssao->NormalMap(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdsLists[] = {_commandList.Get()};
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

    ID3D12DescriptorHeap* descriptorHeaps[] = {_srvHeap.Get()};
    _commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    _commandList->SetGraphicsRootSignature(_rootSignature.Get());

    auto matBuffer = _currFrameResource->MaterialBuffer->Resource();
    _commandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());
    _commandList->SetGraphicsRootDescriptorTable(3, _nullSrv);

    _commandList->SetGraphicsRootDescriptorTable(4, _srvHeap->GetGPUDescriptorHandleForHeapStart());

    DrawSceneToShadowMap();

    DrawNormalsAndDepth();

    _commandList->SetGraphicsRootSignature(_ssaoRootSignature.Get());
    _ssao->ComputeSSAO(_commandList.Get(), _currFrameResource->SSAOCB->Resource()->GetGPUVirtualAddress(), 3);

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
    ID3D12CommandList* cmdLists[] = {_commandList.Get()};
    _commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    ThrowIfFailed(_swapChain->Present(0, 0));
    _currBackBuffer = (_currBackBuffer + 1) % _swapChainBufferCount;

    _currFrameResource->Fence = ++_currentFence;

    _commandQueue->Signal(_fence.Get(), _currentFence);
}

void SSAOScene::OnMouseDown(WPARAM btnState, int x, int y)
{
    _lastMousePos.x = x;
    _lastMousePos.y = y;
    SetCapture(_hMainWindow);
}

void SSAOScene::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void SSAOScene::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - _lastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - _lastMousePos.y));

        _camera.Pitch(dy);
        _camera.RotateY(dx);
    }
    _lastMousePos.x = x;
    _lastMousePos.y = y;
}

void SSAOScene::OnKeyboardInput(const GameTimer& timer)
{
    const float dt = timer.DeltaTime();

    if (GetAsyncKeyState('W') & 0x8000)
        _camera.Walk(10.0f * dt);

    if (GetAsyncKeyState('S') & 0x8000)
        _camera.Walk(-10.0f * dt);

    if (GetAsyncKeyState('A') & 0x8000)
        _camera.Strafe(-10.0f * dt);

    if (GetAsyncKeyState('D') & 0x8000)
        _camera.Strafe(10.0f * dt);

    _camera.UpdateViewMatrix();
}

void SSAOScene::AnimateMaterials(const GameTimer& timer)
{
}

void SSAOScene::UpdateObjectCBs(const GameTimer& timer)
{
    auto currObjectCB = _currFrameResource->ObjectCB.get();
    for (auto& e : _allRenderItems)
    {
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX model = XMLoadFloat4x4(&e->Model);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

            FrameResource::ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.Model, XMMatrixTranspose(model));
            XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
            objConstants.MaterialIndex = e->Mat->MatCBIndex;

            currObjectCB->CopyData(e->ObjCBIndex, objConstants);
            e->NumFramesDirty--;
        }
    }
}

void SSAOScene::UpdateMaterialBuffer(const GameTimer& timer)
{
    auto currMaterialBuffer = _currFrameResource->MaterialBuffer.get();
    for (auto& e : _materials)
    {
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);
            FrameResource::MaterialData matData;
            matData.DiffuseAlbedo = mat->DiffuseAlbedo;
            matData.FresnelR0 = mat->FresnelR0;
            matData.Roughness = mat->Roughness;
            XMStoreFloat4x4(&mat->MatTransform, XMMatrixTranspose(matTransform));
            matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
            matData.NormalMapIndex = mat->NormalSrvHeapIndex;

            currMaterialBuffer->CopyData(mat->MatCBIndex, matData);
            mat->NumFramesDirty--;
        }
    }
}

void SSAOScene::UpdateShadowTransform(const GameTimer& timer)
{
    XMVECTOR lightDir = XMLoadFloat3(&_rotatedLightDirections[0]);
    XMVECTOR lightPos = -2.0f * _sceneBounds.Radius * lightDir;
    XMVECTOR targetPos = XMLoadFloat3(&_sceneBounds.Center);
    XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

    XMStoreFloat3(&_lightPosW, lightPos);

    XMFLOAT3 sphereCenterLS;
    XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

    float l = sphereCenterLS.x - _sceneBounds.Radius;
    float b = sphereCenterLS.y - _sceneBounds.Radius;
    float n = sphereCenterLS.z - _sceneBounds.Radius;
    float r = sphereCenterLS.x + _sceneBounds.Radius;
    float t = sphereCenterLS.y + _sceneBounds.Radius;
    float f = sphereCenterLS.z + _sceneBounds.Radius;

    _lightNearZ = n;
    _lightFarZ = f;
    XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

    XMMATRIX T
    (
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f
    );
    XMMATRIX S = lightView * lightProj * T;
    XMStoreFloat4x4(&_lightView, lightView);
    XMStoreFloat4x4(&_lightProj, lightProj);
    XMStoreFloat4x4(&_shadowTransform, S);
}

void SSAOScene::UpdateMainPassCB(const GameTimer& timer)
{
    XMMATRIX view = _camera.GetView();
    XMMATRIX proj = _camera.GetProj();

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMMATRIX T
    (
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f
    );
    XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);
    XMMATRIX shadowTransform = XMLoadFloat4x4(&_shadowTransform);

    XMStoreFloat4x4(&_mainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&_mainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&_mainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&_mainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&_mainPassCB.VP, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&_mainPassCB.InvVP, XMMatrixTranspose(invViewProj));
    XMStoreFloat4x4(&_mainPassCB.ViewProjTex, XMMatrixTranspose(viewProjTex));
    XMStoreFloat4x4(&_mainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));
    _mainPassCB.EyePosW = _camera.GetPosition3f();
    _mainPassCB.RenderTargetSize = XMFLOAT2((float)_clientWidth, (float)_clientHeight);
    _mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / _clientWidth, 1.0f / _clientHeight);
    _mainPassCB.NearZ = 1.0f;
    _mainPassCB.FarZ = 1000.0f;
    _mainPassCB.TotalTime = timer.TotalTime();
    _mainPassCB.DeltaTime = timer.DeltaTime();
    _mainPassCB.AmbientLight = {0.25f, 0.25f, 0.35f, 1.0f};

    _mainPassCB.Lights[0].Direction = _rotatedLightDirections[0];
    _mainPassCB.Lights[0].Strength = {0.4f, 0.4f, 0.5f};
    _mainPassCB.Lights[1].Direction = _rotatedLightDirections[1];
    _mainPassCB.Lights[1].Strength = {0.1f, 0.1f, 0.1f};
    _mainPassCB.Lights[2].Direction = _rotatedLightDirections[2];
    _mainPassCB.Lights[2].Strength = {0.0f, 0.0f, 0.0f};

    auto currPassCB = _currFrameResource->PassCB.get();
    currPassCB->CopyData(0, _mainPassCB);
}

void SSAOScene::UpdateShadowPassCB(const GameTimer& timer)
{
    XMMATRIX view = XMLoadFloat4x4(&_lightView);
    XMMATRIX proj = XMLoadFloat4x4(&_lightProj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    UINT w = _shadowMap->Width();
    UINT h = _shadowMap->Height();

    XMStoreFloat4x4(&_shadowPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&_shadowPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&_shadowPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&_shadowPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&_shadowPassCB.VP, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&_shadowPassCB.InvVP, XMMatrixTranspose(invViewProj));
    _shadowPassCB.EyePosW = _lightPosW;
    _shadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
    _shadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
    _shadowPassCB.NearZ = _lightNearZ;
    _shadowPassCB.FarZ = _lightFarZ;

    auto currPassCB = _currFrameResource->PassCB.get();
    currPassCB->CopyData(1, _shadowPassCB);
}

void SSAOScene::UpdateSSAOCB(const GameTimer& timer)
{
    FrameResource::SSAOConstants ssaoCB;
    XMMATRIX P = _camera.GetProj();

    XMMATRIX T
        (
            0.5f, 0.0f, 0.0f, 0.0f,
            0.0f, -0.5f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.0f, 1.0f
            );
    ssaoCB.Proj = _mainPassCB.Proj;
    ssaoCB.InvProj = _mainPassCB.InvProj;
    XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P * T));
    _ssao->GetOffsetVectors(ssaoCB.OffsetVectors);

    auto blurWeights = _ssao->CalcGaussWeights(2.5f);
    ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
    ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
    ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

    ssaoCB.InvRenderTartetSize = XMFLOAT2(1.0f / _ssao->SSAOMapWidth(), 1.0f / _ssao->SSAOMapHeight());

    ssaoCB.OcclusionRadius = 0.5f;
    ssaoCB.OcclusionFadeStart = 0.2f;
    ssaoCB.OcclusionFadeEnd = 1.0f;
    ssaoCB.SurfaceEpsilon = 0.05f;

    auto currSssaoCB = _currFrameResource->SSAOCB.get();
    currSssaoCB->CopyData(0, ssaoCB);
}

void SSAOScene::LoadTextures()
{
    std::vector<std::string> texNames =
    {
        "bricksDiffuseMap",
        "bricksNormalMap",
        "tileDiffuseMap",
        "tileNormalMap",
        "defaultDiffuseMap",
        "defaultNormalMap",
        "skyCubeMap"
    };
    std::vector<std::wstring> texFilenames =
    {
        L"Textures/bricks2.dds",
        L"Textures/bricks2_nmap.dds",
        L"Textures/tile.dds",
        L"Textures/tile_nmap.dds",
        L"Textures/white1x1.dds",
        L"Textures/default_nmap.dds",
        L"Textures/sunsetcube1024.dds"
    };
    for (int i = 0; i < (int)texNames.size(); i++)
    {
        auto texMap = std::make_unique<Texture>();
        texMap->Name = texNames[i];
        texMap->Filename = texFilenames[i];
        ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), texMap->Filename.c_str(), texMap->Resource, texMap->UploadHeap));
        _textures[texMap->Name] = std::move(texMap);
    }
}

void SSAOScene::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 10, 3, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[5];

	// Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);
    slotRootParameter[2].InitAsShaderResourceView(0, 1);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(_rootSignature.GetAddressOf())));
}

void SSAOScene::BuildSSAORootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable0;
    texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);
    CD3DX12_DESCRIPTOR_RANGE texTable1;
    texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstants(1, 1);
    slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
        0.0f,
        0,
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers =
    {
        pointClamp, linearClamp, depthMapSam, linearWrap
    };

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(_ssaoRootSignature.GetAddressOf())));
}

void SSAOScene::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 18;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(_srvHeap.GetAddressOf())));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(_srvHeap->GetCPUDescriptorHandleForHeapStart());
    
    std::vector<ComPtr<ID3D12Resource>> tex2DList =
    {
        _textures["bricksDiffuseMap"]->Resource,
        _textures["bricksNormalMap"]->Resource,
        _textures["tileDiffuseMap"]->Resource,
        _textures["tileNormalMap"]->Resource,
        _textures["defaultDiffuseMap"]->Resource,
        _textures["defaultNormalMap"]->Resource
    };

    auto skyCubeMap = _textures["skyCubeMap"]->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    for (UINT i = 0; i < (UINT)tex2DList.size(); i++)
    {
        srvDesc.Format = tex2DList[i]->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
        _device->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);
        hDescriptor.Offset(1, _cbvSrvUavDescriptorSize);
    }

    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = skyCubeMap->GetDesc().MipLevels;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    srvDesc.Format = skyCubeMap->GetDesc().Format;
    _device->CreateShaderResourceView(skyCubeMap.Get(), &srvDesc, hDescriptor);

    _skyTexHeapIndex = (UINT)tex2DList.size();
    _shadowMapHeapIndex = _skyTexHeapIndex + 1;
    _ssaoHeapIndex = _shadowMapHeapIndex + 1;
    _ssaoAmbientMapIndex = _ssaoHeapIndex + 3;
    _nullCubeSrvIndex = _ssaoHeapIndex + 5;
    _nullTexSrvIndex1 = _nullCubeSrvIndex + 1;
    _nullTexSrvIndex2 = _nullTexSrvIndex1 + 1;

    auto nullSrv = GetCpuSrv(_nullCubeSrvIndex);
    _nullSrv = GetGpuSrv(_nullCubeSrvIndex);

    _device->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);
    nullSrv.Offset(1, _cbvSrvUavDescriptorSize);

    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    _device->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

    _nullSrv.Offset(1, _cbvSrvUavDescriptorSize);
    _device->CreateShaderResourceView(nullptr, &srvDesc, nullSrv);

    _shadowMap->BuildDescriptors(GetCpuSrv(_shadowMapHeapIndex), GetGpuSrv(_shadowMapHeapIndex), GetDsv(1));
    _ssao->BuildDescriptors(_depthStencilBuffer.Get(), GetCpuSrv(_ssaoHeapIndex), GetGpuSrv(_ssaoHeapIndex), GetRtv(_swapChainBufferCount), _cbvSrvUavDescriptorSize, _rtvDescriptorSize);
}

void SSAOScene::BuildShaderAndInputLayout()
{
    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "ALPHA_TEST", "1",
        NULL, NULL
    };

    _shaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAODefault.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["opaquePS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAODefault.hlsl", nullptr, "frag", "ps_5_1");

    _shaders["shadowVS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\RenderShadows.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["shadowOpaquePS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\RenderShadows.hlsl", nullptr, "frag", "ps_5_1");
    _shaders["shadowAlphaTestedPS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\RenderShadows.hlsl", alphaTestDefines, "frag", "ps_5_1");

    _shaders["debugVS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAODebug.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["debugPS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAODebug.hlsl", nullptr, "frag", "ps_5_1");

    _shaders["drawNormalsVS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\DrawNormals.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["drawNormalsPS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\DrawNormals.hlsl", nullptr, "frag", "ps_5_1");

    _shaders["ssaoVS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAO.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["ssaoPS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAO.hlsl", nullptr, "frag", "ps_5_1");

    _shaders["ssaoBlurVS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAOBlur.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["ssaoBlurPS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAOBlur.hlsl", nullptr, "frag", "ps_5_1");

    _shaders["skyVS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\Sky.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["skyPS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\Sky.hlsl", nullptr, "frag", "ps_5_1");

    _inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void SSAOScene::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
    GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

    UINT boxVertexOffset = 0;
    UINT gridVertexOffset = (UINT)box.Vertices.size();
    UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
    UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
    UINT quadVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();

    UINT boxIndexOffset = 0;
    UINT gridIndexOffset = (UINT)box.Indices32.size();
    UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
    UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
    UINT quadIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = boxIndexOffset;
    boxSubmesh.BaseVertexLocation = boxVertexOffset;

    SubmeshGeometry gridSubmesh;
    gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
    gridSubmesh.StartIndexLocation = gridIndexOffset;
    gridSubmesh.BaseVertexLocation = gridVertexOffset;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    SubmeshGeometry cylinderSubmesh;
    cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
    cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
    cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    SubmeshGeometry quadSubmesh;
    quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
    quadSubmesh.StartIndexLocation = quadIndexOffset;
    quadSubmesh.BaseVertexLocation = quadVertexOffset;

    auto totalVertexCount =
        box.Vertices.size() +
        grid.Vertices.size() +
        sphere.Vertices.size() +
        cylinder.Vertices.size() +
        quad.Vertices.size();

    std::vector<Vertex> vertices(totalVertexCount);

    UINT k = 0;
    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = box.Vertices[i].Position;
        vertices[k].Normal = box.Vertices[i].Normal;
        vertices[k].Uv = box.Vertices[i].TexCoord;
        vertices[k].Tangent = box.Vertices[i].Tangent;
    }

    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = grid.Vertices[i].Position;
        vertices[k].Normal = grid.Vertices[i].Normal;
        vertices[k].Uv = grid.Vertices[i].TexCoord;
        vertices[k].Tangent = grid.Vertices[i].Tangent;
    }

    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = sphere.Vertices[i].Position;
        vertices[k].Normal = sphere.Vertices[i].Normal;
        vertices[k].Uv = sphere.Vertices[i].TexCoord;
        vertices[k].Tangent = sphere.Vertices[i].Tangent;
    }

    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = cylinder.Vertices[i].Position;
        vertices[k].Normal = cylinder.Vertices[i].Normal;
        vertices[k].Uv = cylinder.Vertices[i].TexCoord;
        vertices[k].Tangent = cylinder.Vertices[i].Tangent;
    }

    for (size_t i = 0; i < quad.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = quad.Vertices[i].Position;
        vertices[k].Normal = quad.Vertices[i].Normal;
        vertices[k].Uv = quad.Vertices[i].TexCoord;
        vertices[k].Tangent = quad.Vertices[i].Tangent;
    }

    std::vector<uint16_t> indices;
    indices.insert(indices.end(), begin(box.GetIndices16()), end(box.GetIndices16()));
    indices.insert(indices.end(), begin(grid.GetIndices16()), end(grid.GetIndices16()));
    indices.insert(indices.end(), begin(sphere.GetIndices16()), end(sphere.GetIndices16()));
    indices.insert(indices.end(), begin(cylinder.GetIndices16()), end(cylinder.GetIndices16()));
    indices.insert(indices.end(), begin(quad.GetIndices16()), end(quad.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size()  * sizeof(uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "shapeGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
        _commandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(),
        _commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["box"] = boxSubmesh;
    geo->DrawArgs["grid"] = gridSubmesh;
    geo->DrawArgs["sphere"] = sphereSubmesh;
    geo->DrawArgs["cylinder"] = cylinderSubmesh;
    geo->DrawArgs["quad"] = quadSubmesh;

    _geometries[geo->Name] = move(geo);
}

void SSAOScene::BuildSkullGeometry()
{
    std::ifstream fin("Models\\skull.txt");

    if (!fin)
    {
        MessageBox(0, L"Models\\scull.txt not found", nullptr, 0);
        return;
    }

    UINT vcount = 0;
    UINT tcount = 0;
    std::string ignore;

    fin >> ignore >> vcount;
    fin >> ignore >> tcount;
    fin >> ignore >> ignore >> ignore >> ignore;

    XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
    XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

    XMVECTOR vMin = XMLoadFloat3(&vMinf3);
    XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

    std::vector<Vertex> vertices(vcount);
    for (UINT i = 0; i < vcount; i++)
    {
        fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
        fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

        XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);
        XMVECTOR N = XMLoadFloat3(&vertices[i].Normal);
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        if (fabsf(XMVectorGetX(XMVector3Dot(N, up))) < 1.0f - 0.001f)
        {
            XMVECTOR T = XMVector3Normalize(XMVector3Cross(up, N));
            XMStoreFloat3(&vertices[i].Tangent, T);
        }
        else
        {
            up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
            XMVECTOR T = XMVector3Normalize(XMVector3Cross(N, up));
            XMStoreFloat3(&vertices[i].Tangent, T);
        }

        XMFLOAT3 spherePos;
        XMStoreFloat3(&spherePos, XMVector3Normalize(P));

        float theta = atan2f(spherePos.z, spherePos.x);

        if (theta < 0.0f)
            theta += XM_2PI;

        float phi = acosf(spherePos.y);

        float u = theta / (2.0f * XM_PI);
        float v = phi / XM_PI;

        vertices[i].Uv = { u, v };

        vMin = XMVectorMin(vMin, P);
        vMax = XMVectorMax(vMax, P);
    }
    BoundingBox bounds;
    XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
    XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));

    fin >> ignore;
    fin >> ignore;
    fin >> ignore;

    std::vector<std::int32_t> indices(3 * tcount);
    for (UINT i = 0; i < tcount; i++)
    {
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    }
    fin.close();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(int32_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "skullGeo";

    geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    geo->DrawArgs["skull"] = submesh;
    _geometries[geo->Name] = move(geo);
}

void SSAOScene::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC basePsoDesc;

    ZeroMemory(&basePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    basePsoDesc.InputLayout = { _inputLayout.data(), (UINT)_inputLayout.size() };
    basePsoDesc.pRootSignature = _rootSignature.Get();
    basePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(_shaders["standardVS"]->GetBufferPointer()),
        _shaders["standardVS"]->GetBufferSize()
    };
    basePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["opaquePS"]->GetBufferPointer()),
        _shaders["opaquePS"]->GetBufferSize()
    };
    basePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    basePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    basePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    basePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    basePsoDesc.SampleMask = UINT_MAX;
    basePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    basePsoDesc.NumRenderTargets = 1;
    basePsoDesc.RTVFormats[0] = _backBufferFormat;
    basePsoDesc.SampleDesc.Count = _4xMsaa ? 4 : 1;
    basePsoDesc.SampleDesc.Quality = _4xMsaa ? (_4xMsaaQuality - 1) : 0;
    basePsoDesc.DSVFormat = _dsvFormat;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePso = basePsoDesc;
    opaquePso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
    opaquePso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&opaquePso, IID_PPV_ARGS(&_PSOs["opaque"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = basePsoDesc;
    smapPsoDesc.RasterizerState.DepthBias = 100000;
    smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
    smapPsoDesc.pRootSignature = _rootSignature.Get();
    smapPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(_shaders["shadowVS"]->GetBufferPointer()),
        _shaders["shadowVS"]->GetBufferSize()
    };
    smapPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["shadowOpaquePS"]->GetBufferPointer()),
        _shaders["shadowOpaquePS"]->GetBufferSize()
    };
    smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    smapPsoDesc.NumRenderTargets = 0;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&_PSOs["shadow_opaque"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = basePsoDesc;
    debugPsoDesc.pRootSignature = _rootSignature.Get();
    debugPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(_shaders["debugVS"]->GetBufferPointer()),
        _shaders["debugVS"]->GetBufferSize()
    };
    debugPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["debugPS"]->GetBufferPointer()),
        _shaders["debugPS"]->GetBufferSize()
    };
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&_PSOs["debug"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalsPsoDesc = basePsoDesc;
    drawNormalsPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(_shaders["drawNormalsVS"]->GetBufferPointer()),
        _shaders["drawNormalsVS"]->GetBufferSize()
    };
    drawNormalsPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["drawNormalsPS"]->GetBufferPointer()),
        _shaders["drawNormalsPS"]->GetBufferSize()
    };
    drawNormalsPsoDesc.RTVFormats[0] = SSAO::NormalMapFormat;
    drawNormalsPsoDesc.SampleDesc.Count = 1;
    drawNormalsPsoDesc.SampleDesc.Quality = 0;
    drawNormalsPsoDesc.DSVFormat = _dsvFormat;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&drawNormalsPsoDesc, IID_PPV_ARGS(&_PSOs["drawNormals"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = basePsoDesc;
    ssaoPsoDesc.InputLayout = { nullptr, 0 };
    ssaoPsoDesc.pRootSignature = _ssaoRootSignature.Get();
    ssaoPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(_shaders["ssaoVS"]->GetBufferPointer()),
        _shaders["ssaoVS"]->GetBufferSize()
    };
    ssaoPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["ssaoPS"]->GetBufferPointer()),
        _shaders["ssaoPS"]->GetBufferSize()
    };
    ssaoPsoDesc.DepthStencilState.DepthEnable = false;
    ssaoPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    ssaoPsoDesc.RTVFormats[0] = SSAO::AmbientMapFormat;
    ssaoPsoDesc.SampleDesc.Count = 1;
    ssaoPsoDesc.SampleDesc.Quality = 0;
    ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(&_PSOs["ssao"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoBlurPsoDesc = ssaoPsoDesc;
    ssaoBlurPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(_shaders["ssaoBlurVS"]->GetBufferPointer()),
        _shaders["ssaoBlurVS"]->GetBufferSize()
    };
    ssaoBlurPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["ssaoBlurPS"]->GetBufferPointer()),
        _shaders["ssaoBlurPS"]->GetBufferSize()
    };
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&ssaoBlurPsoDesc, IID_PPV_ARGS(&_PSOs["ssaoBlur"])));


    D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = basePsoDesc;
    skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    skyPsoDesc.pRootSignature = _rootSignature.Get();
    skyPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(_shaders["skyVS"]->GetBufferPointer()),
        _shaders["skyVS"]->GetBufferSize()
    };
    skyPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["skyPS"]->GetBufferPointer()),
        _shaders["skyPS"]->GetBufferSize()
    };
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&_PSOs["sky"])));
}

void SSAOScene::BuildFrameResources()
{
    for (int i = 0; i < FrameResource::NumFrameResources; i++)
        _frameResources.push_back(std::make_unique<FrameResource>(_device.Get(), 2, (UINT)_allRenderItems.size(), (UINT)_materials.size()));
}

void SSAOScene::BuildMaterials()
{
    auto bricks0 = std::make_unique<Material>();
    bricks0->Name = "bricks0";
    bricks0->MatCBIndex = 0;
    bricks0->DiffuseSrvHeapIndex = 0;
    bricks0->NormalSrvHeapIndex = 1;
    bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bricks0->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    bricks0->Roughness = 0.3f;

    auto tile0 = std::make_unique<Material>();
    tile0->Name = "tile0";
    tile0->MatCBIndex = 1;
    tile0->DiffuseSrvHeapIndex = 2;
    tile0->NormalSrvHeapIndex = 3;
    tile0->DiffuseAlbedo = XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);
    tile0->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
    tile0->Roughness = 0.1f;

    auto mirror0 = std::make_unique<Material>();
    mirror0->Name = "mirror0";
    mirror0->MatCBIndex = 2;
    mirror0->DiffuseSrvHeapIndex = 4;
    mirror0->NormalSrvHeapIndex = 5;
    mirror0->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.1f, 1.0f);
    mirror0->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
    mirror0->Roughness = 0.1f;

    auto skullMat = std::make_unique<Material>();
    skullMat->Name = "skullMat";
    skullMat->MatCBIndex = 3;
    skullMat->DiffuseSrvHeapIndex = 4;
    skullMat->NormalSrvHeapIndex = 5;
    skullMat->DiffuseAlbedo = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
    skullMat->FresnelR0 = XMFLOAT3(0.6f, 0.6f, 0.6f);
    skullMat->Roughness = 0.2f;

    auto sky = std::make_unique<Material>();
    sky->Name = "sky";
    sky->MatCBIndex = 4;
    sky->DiffuseSrvHeapIndex = 6;
    sky->NormalSrvHeapIndex = 7;
    sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    sky->Roughness = 1.0f;

    _materials["bricks0"] = move(bricks0);
    _materials["tile0"] = move(tile0);
    _materials["mirror0"] = move(mirror0);
    _materials["skullMat"] = move(skullMat);
    _materials["sky"] = move(sky);
}

void SSAOScene::BuildRenderItems()
{
    auto skyRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&skyRitem->Model, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyRitem->TexTransform = MathHelper::Identity4x4();
    skyRitem->ObjCBIndex = 0;
    skyRitem->Mat = _materials["sky"].get();
    skyRitem->Geo = _geometries["shapeGeo"].get();
    skyRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
    skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
    skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
    _renderItemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());
    _allRenderItems.push_back(move(skyRitem));

    auto quadRitem = std::make_unique<RenderItem>();
    quadRitem->Model = MathHelper::Identity4x4();
    quadRitem->TexTransform = MathHelper::Identity4x4();
    quadRitem->ObjCBIndex = 1;
    quadRitem->Mat = _materials["bricks0"].get();
    quadRitem->Geo = _geometries["shapeGeo"].get();
    quadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;
    quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
    quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;
    _renderItemLayer[(int)RenderLayer::Debug].push_back(quadRitem.get());
    _allRenderItems.push_back(move(quadRitem));

    auto boxRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&boxRitem->Model, XMMatrixScaling(2.0f, 1.0f, 2.0f)*XMMatrixTranslation(0.0f, 0.5f, 0.0f));
    XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 0.5f, 1.0f));
    boxRitem->ObjCBIndex = 2;
    boxRitem->Mat = _materials["bricks0"].get();
    boxRitem->Geo = _geometries["shapeGeo"].get();
    boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
    _renderItemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());
    _allRenderItems.push_back(move(boxRitem));

    auto skullRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&skullRitem->Model, XMMatrixScaling(0.4f, 0.4f, 0.4f)*XMMatrixTranslation(0.0f, 1.0f, 0.0f));
    skullRitem->TexTransform = MathHelper::Identity4x4();
    skullRitem->ObjCBIndex = 3;
    skullRitem->Mat = _materials["skullMat"].get();
    skullRitem->Geo = _geometries["skullGeo"].get();
    skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
    skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
    skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
    _renderItemLayer[(int)RenderLayer::Opaque].push_back(skullRitem.get());
    _allRenderItems.push_back(move(skullRitem));

    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->Model = MathHelper::Identity4x4();
    XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
    gridRitem->ObjCBIndex = 4;
    gridRitem->Mat = _materials["tile0"].get();
    gridRitem->Geo = _geometries["shapeGeo"].get();
    gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
    _renderItemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
    _allRenderItems.push_back(move(gridRitem));

    XMMATRIX brickTexTransform = XMMatrixScaling(1.5f, 2.0f, 1.0f);
    UINT objCBIndex = 5;
    for (int i = 0; i < 5; ++i)
    {
        auto leftCylRitem = std::make_unique<RenderItem>();
        auto rightCylRitem = std::make_unique<RenderItem>();
        auto leftSphereRitem = std::make_unique<RenderItem>();
        auto rightSphereRitem = std::make_unique<RenderItem>();

        XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i*5.0f);
        XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i*5.0f);

        XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i*5.0f);
        XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i*5.0f);

        XMStoreFloat4x4(&leftCylRitem->Model, rightCylWorld);
        XMStoreFloat4x4(&leftCylRitem->TexTransform, brickTexTransform);
        leftCylRitem->ObjCBIndex = objCBIndex++;
        leftCylRitem->Mat = _materials["bricks0"].get();
        leftCylRitem->Geo = _geometries["shapeGeo"].get();
        leftCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
        leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
        leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

        XMStoreFloat4x4(&rightCylRitem->Model, leftCylWorld);
        XMStoreFloat4x4(&rightCylRitem->TexTransform, brickTexTransform);
        rightCylRitem->ObjCBIndex = objCBIndex++;
        rightCylRitem->Mat = _materials["bricks0"].get();
        rightCylRitem->Geo = _geometries["shapeGeo"].get();
        rightCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
        rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
        rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

        XMStoreFloat4x4(&leftSphereRitem->Model, leftSphereWorld);
        leftSphereRitem->TexTransform = MathHelper::Identity4x4();
        leftSphereRitem->ObjCBIndex = objCBIndex++;
        leftSphereRitem->Mat = _materials["mirror0"].get();
        leftSphereRitem->Geo = _geometries["shapeGeo"].get();
        leftSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
        leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
        leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

        XMStoreFloat4x4(&rightSphereRitem->Model, rightSphereWorld);
        rightSphereRitem->TexTransform = MathHelper::Identity4x4();
        rightSphereRitem->ObjCBIndex = objCBIndex++;
        rightSphereRitem->Mat = _materials["mirror0"].get();
        rightSphereRitem->Geo = _geometries["shapeGeo"].get();
        rightSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
        rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
        rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

        _renderItemLayer[(int)RenderLayer::Opaque].push_back(leftCylRitem.get());
        _renderItemLayer[(int)RenderLayer::Opaque].push_back(rightCylRitem.get());
        _renderItemLayer[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
        _renderItemLayer[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());

        _allRenderItems.push_back(move(leftCylRitem));
        _allRenderItems.push_back(move(rightCylRitem));
        _allRenderItems.push_back(move(leftSphereRitem));
        _allRenderItems.push_back(move(rightSphereRitem));
    }
}

void SSAOScene::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems)
{
    UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(FrameResource::ObjectConstants));


    auto objectCB = _currFrameResource->ObjectCB->Resource();

    for (size_t i = 0; i < renderItems.size(); i++)
    {
        auto ri = renderItems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void SSAOScene::DrawSceneToShadowMap()
{
    _commandList->RSSetViewports(1, &_shadowMap->Viewport());
    _commandList->RSSetScissorRects(1, &_shadowMap->ScissorRect());

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_shadowMap->Resource(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    UINT passCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(FrameResource::PassConstants));
    _commandList->ClearDepthStencilView(_shadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    _commandList->OMSetRenderTargets(0, nullptr, false, &_shadowMap->Dsv());

    auto passCB = _currFrameResource->PassCB->Resource();
    D3D12_GPU_VIRTUAL_ADDRESS passCBAdderss = passCB->GetGPUVirtualAddress() + passCBByteSize;
    _commandList->SetGraphicsRootConstantBufferView(1, passCBAdderss);

    _commandList->SetPipelineState(_PSOs["shadow_opaque"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::Opaque]);
    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_shadowMap->Resource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SSAOScene::DrawNormalsAndDepth()
{
    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);

    auto normalMap = _ssao->NormalMap();
    auto normalMapRtv = _ssao->NormalMapRtv();

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

    float clearValue[] = { 0.0f, 0.0f, 1.0f, 0.0 };
    _commandList->ClearRenderTargetView(normalMapRtv, clearValue, 0, nullptr);
    _commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    _commandList->OMSetRenderTargets(1, &normalMapRtv, true, &DepthStencilView());

    auto passCB = _currFrameResource->PassCB->Resource();
    _commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

    _commandList->SetPipelineState(_PSOs["drawNormals"].Get());

    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::Opaque]);
    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SSAOScene::GetCpuSrv(int index) const
{
    auto srv = CD3DX12_CPU_DESCRIPTOR_HANDLE(_srvHeap->GetCPUDescriptorHandleForHeapStart());
    srv.Offset(index, _cbvSrvUavDescriptorSize);
    return srv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SSAOScene::GetGpuSrv(int index) const
{
    auto srv = CD3DX12_GPU_DESCRIPTOR_HANDLE(_srvHeap->GetGPUDescriptorHandleForHeapStart());
    srv.Offset(index, _cbvSrvUavDescriptorSize);
    return srv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SSAOScene::GetDsv(int index) const
{
    auto dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    dsv.Offset(index, _dsvDescriptorSize);
    return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SSAOScene::GetRtv(int index) const
{
    auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    rtv.Offset(index, _rtvDescriptorSize);
    return rtv;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> SSAOScene::GetStaticSamplers()
{
    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressW
        0.0f, // mipLODBias
        8); // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressW
        0.0f, // mipLODBias
        8); // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC shadow(
        6, // shaderRegister
        D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressU
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressV
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressW
        0.0f, // mipLODBias
        16, // maxAnisotropy
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp,
        shadow
    };
}
}