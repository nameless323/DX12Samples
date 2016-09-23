#include "SkinnedAnimaiton.h"

#include "../../../Core/GeometryGenerator.h"
#include <minwinbase.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;
using Vertex = SkinnedAnimFrameResource::Vertex;
using FrameResource = SkinnedAnimFrameResource;
using RenderLayer = RenderItem::RenderLayer;



SkinnedAnimation::SkinnedAnimation(HINSTANCE hInstance) : Application(hInstance)
{
    _sceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    _sceneBounds.Radius = sqrtf(10.0f * 10.0f + 15.0f * 15.0f);
}

bool SkinnedAnimation::Init()
{
    if (!Application::Init())
        return false;
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    _camera.SetPosition(0.0f, 2.0f, -15.0f);

    _shadowMap = std::make_unique<ShadowMap>(_device.Get(), 2048, 2048);
    _ssao = std::make_unique<SSAO>(_device.Get(), _commandList.Get(), _clientWidth, _clientHeight);

    LoadSkinnedModel();
    LoadTextures();
    BuildRootSignature();
    BuildSSAORootSignature();
    BuildDescriptorHeaps();
    BuildShaderAndInputLayout();
    BuildShapeGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    _ssao->SetPSOs(_PSOs["ssao"].Get(), _PSOs["ssaoBlur"].Get());


    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_ssao->NormalMap(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    FlushCommandQueue();

    return true;
}

SkinnedAnimation::~SkinnedAnimation()
{
    if (_device != nullptr)
        FlushCommandQueue();
}

LRESULT SkinnedAnimation::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int SkinnedAnimation::Run()
{
    return Application::Run();
}

void SkinnedAnimation::CreateRtvAndDsvDescriptorHeaps()
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

void SkinnedAnimation::OnResize()
{
    Application::OnResize();
    _camera.SetFrustum(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    if (_ssao != nullptr)
    {
        _ssao->OnResize(_clientWidth, _clientHeight);
        _ssao->RebuildDescriptors(_depthStencilBuffer.Get());
    }
}

void SkinnedAnimation::Update(const GameTimer& timer)
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

void SkinnedAnimation::Draw(const GameTimer& timer)
{
    auto cmdListAlloc = _currFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(_commandList->Reset(cmdListAlloc.Get(), _PSOs["opaque"].Get()));

    ID3D12DescriptorHeap* descriptorHeaps[] = { _srvHeap.Get() };
    _commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    _commandList->SetGraphicsRootSignature(_rootSignature.Get());

    auto matBuffer = _currFrameResource->MaterialBuffer->Resource();
    _commandList->SetGraphicsRootShaderResourceView(3, matBuffer->GetGPUVirtualAddress());
    _commandList->SetGraphicsRootDescriptorTable(4, _nullSrv);

    _commandList->SetGraphicsRootDescriptorTable(5, _srvHeap->GetGPUDescriptorHandleForHeapStart());

    DrawSceneToShadowMap();

    DrawNormalsAndDepth();

    _commandList->SetGraphicsRootSignature(_ssaoRootSignature.Get());
    _ssao->ComputeSSAO(_commandList.Get(), _currFrameResource->SSAOCB->Resource()->GetGPUVirtualAddress(), 2);

    _commandList->SetGraphicsRootSignature(_rootSignature.Get());

    matBuffer = _currFrameResource->MaterialBuffer->Resource();
    _commandList->SetGraphicsRootShaderResourceView(3, matBuffer->GetGPUVirtualAddress());

    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    _commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    _commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    _commandList->SetGraphicsRootDescriptorTable(5, _srvHeap->GetGPUDescriptorHandleForHeapStart());

    auto passCB = _currFrameResource->PassCB->Resource();
    _commandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    CD3DX12_GPU_DESCRIPTOR_HANDLE skyTexDescriptor(_srvHeap->GetGPUDescriptorHandleForHeapStart());
    skyTexDescriptor.Offset(_skyTexHeapIndex, _cbvSrvUavDescriptorSize);
    _commandList->SetGraphicsRootDescriptorTable(4, skyTexDescriptor);

    _commandList->SetPipelineState(_PSOs["opaque"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::Opaque]);

    _commandList->SetPipelineState(_PSOs["skinnedOpaque"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::SkinnedOpaque]);

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

void SkinnedAnimation::OnMouseDown(WPARAM btnState, int x, int y)
{
    _lastMousePos.x = x;
    _lastMousePos.y = y;
    SetCapture(_hMainWindow);
}

void SkinnedAnimation::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void SkinnedAnimation::OnMouseMove(WPARAM btnState, int x, int y)
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

void SkinnedAnimation::OnKeyboardInput(const GameTimer& timer)
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

void SkinnedAnimation::AnimateMaterials(const GameTimer& timer)
{
}

void SkinnedAnimation::UpdateObjectCBs(const GameTimer& timer)
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

void SkinnedAnimation::UpdateSkinnedCBs(const GameTimer& timer)
{
    auto currSkinnedCB = _currFrameResource->SkinnedCB.get();

    _skinnedModelInst->UpdateSkinnedAnimation(timer.DeltaTime());

    SkinnedAnimFrameResource::SkinnedConstants skinnedConstants;
    for (int i = 0; i < _skinnedModelInst->FinalTransforms.size(); i++)
        skinnedConstants.BoneTransforms[i] = _skinnedModelInst->FinalTransforms[i];
//    std::copy(std::begin(_skinnedModelInst->FinalTransforms), std::end(_skinnedModelInst->FinalTransforms), &skinnedConstants.BoneTransforms[0]);
    currSkinnedCB->CopyData(0, skinnedConstants);
}

void SkinnedAnimation::UpdateMaterialBuffer(const GameTimer& timer)
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

void SkinnedAnimation::UpdateShadowTransform(const GameTimer& timer)
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

void SkinnedAnimation::UpdateMainPassCB(const GameTimer& timer)
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
    _mainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

    _mainPassCB.Lights[0].Direction = _rotatedLightDirections[0];
    _mainPassCB.Lights[0].Strength = { 0.4f, 0.4f, 0.5f };
    _mainPassCB.Lights[1].Direction = _rotatedLightDirections[1];
    _mainPassCB.Lights[1].Strength = { 0.1f, 0.1f, 0.1f };
    _mainPassCB.Lights[2].Direction = _rotatedLightDirections[2];
    _mainPassCB.Lights[2].Strength = { 0.0f, 0.0f, 0.0f };

    auto currPassCB = _currFrameResource->PassCB.get();
    currPassCB->CopyData(0, _mainPassCB);
}

void SkinnedAnimation::UpdateShadowPassCB(const GameTimer& timer)
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

void SkinnedAnimation::UpdateSSAOCB(const GameTimer& timer)
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

void SkinnedAnimation::LoadTextures()
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

    for (UINT i = 0; i < _skinnedMats.size(); i++)
    {
        std::string diffuseName = _skinnedMats[i].DiffuseMapName;
        std::string normalName = _skinnedMats[i].NormalMapName;

        std::wstring diffuseFilename = L"Textures/" + AnsiToWString(diffuseName);
        std::wstring normalFilename = L"Textures/" + AnsiToWString(normalName);

        diffuseName = diffuseName.substr(0, diffuseName.find_last_of("."));
        normalName = normalName.substr(0, normalName.find_last_of("."));

        _skinnedTextureNames.push_back(diffuseName);
        texNames.push_back(diffuseName);
        texFilenames.push_back(diffuseFilename);

        _skinnedTextureNames.push_back(normalName);
        texNames.push_back(normalName);
        texFilenames.push_back(normalFilename);
    }
    for (int i = 0; i < (int)texNames.size(); i++)
    {
        if (_textures.find(texNames[i]) == std::end(_textures))
        {
            auto texMap = std::make_unique<Texture>();
            texMap->Name = texNames[i];
            texMap->Filename = texFilenames[i];
            ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), texMap->Filename.c_str(), texMap->Resource, texMap->UploadHeap));
            _textures[texMap->Name] = std::move(texMap);
        }
    }
}

void SkinnedAnimation::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable0;
    texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0);

    CD3DX12_DESCRIPTOR_RANGE texTable1;
    texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 48, 3, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[6];

    // Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);
    slotRootParameter[2].InitAsConstantBufferView(2);
    slotRootParameter[3].InitAsShaderResourceView(0, 1);
    slotRootParameter[4].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[5].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(_rootSignature.GetAddressOf())));
}

void SkinnedAnimation::BuildSSAORootSignature()
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

void SkinnedAnimation::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 64;
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

    _skinnedSrvHeapStart = (UINT)tex2DList.size();
    for (UINT i = 0; i < (UINT)_skinnedTextureNames.size(); i++)
    {
        auto texResource = _textures[_skinnedTextureNames[i]]->Resource;
        assert(texResource != nullptr);
        tex2DList.push_back(texResource);
    }

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

void SkinnedAnimation::BuildShaderAndInputLayout()
{
    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "ALPHA_TEST", "1",
        NULL, NULL
    };

    const D3D_SHADER_MACRO skinnedDefines[] =
    {
        "SKINNED", "1",
        NULL, NULL
    };


    _shaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\Default.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["skinnedVS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\Default.hlsl", skinnedDefines, "vert", "vs_5_1");
    _shaders["opaquePS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\Default.hlsl", nullptr, "frag", "ps_5_1");

    _shaders["shadowVS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\RenderShadows.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["skinnedShadowVS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\RenderShadows.hlsl", skinnedDefines, "vert", "vs_5_1");
    _shaders["shadowOpaquePS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\RenderShadows.hlsl", nullptr, "frag", "ps_5_1");
    _shaders["shadowAlphaTestedPS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\RenderShadows.hlsl", alphaTestDefines, "frag", "ps_5_1");

    _shaders["debugVS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\Debug.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["debugPS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\Debug.hlsl", nullptr, "frag", "ps_5_1");

    _shaders["drawNormalsVS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\DrawNormals.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["skinnedDrawNormalsVS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\DrawNormals.hlsl", skinnedDefines, "vert", "vs_5_1");
    _shaders["drawNormalsPS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\DrawNormals.hlsl", nullptr, "frag", "ps_5_1");

    _shaders["ssaoVS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAO.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["ssaoPS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAO.hlsl", nullptr, "frag", "ps_5_1");

    _shaders["ssaoBlurVS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAOBlur.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["ssaoBlurPS"] = D3DUtil::CompileShader(L"Shaders\\SSAO\\SSAOBlur.hlsl", nullptr, "frag", "ps_5_1");

    _shaders["skyVS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\Sky.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["skyPS"] = D3DUtil::CompileShader(L"Shaders\\SkinnedRender\\Sky.hlsl", nullptr, "frag", "ps_5_1");

    _inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }   
    }; 
    _skinnedInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void SkinnedAnimation::BuildShapeGeometry()
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

void SkinnedAnimation::LoadSkinnedModel()
{
    std::vector<M3dLoader::SkinnedVertex> vertices;
    std::vector<std::uint16_t> indices;

    M3dLoader m3dLoader;
    m3dLoader.LoadM3d(_skinnedModelFilename, vertices, indices, _skinnedSubsets, _skinnedMats, _skinnedInfo);

    _skinnedModelInst = std::make_unique<SkinnedModelInstance>();
    _skinnedModelInst->SkinnedInfo = &_skinnedInfo;
    _skinnedModelInst->FinalTransforms.resize(_skinnedInfo.BoneCount());
    _skinnedModelInst->ClipName = "Take1";
    _skinnedModelInst->TimePos = 0.0f;

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(M3dLoader::SkinnedVertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = _skinnedModelFilename;

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(M3dLoader::SkinnedVertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    for (UINT i = 0; i < (UINT)_skinnedSubsets.size(); i++)
    {
        SubmeshGeometry submesh;
        std::string name = "sm_" + std::to_string(i);

        submesh.IndexCount = (UINT)_skinnedSubsets[i].FaceCount * 3;
        submesh.StartIndexLocation = _skinnedSubsets[i].FaceStart * 3;
        submesh.BaseVertexLocation = 0;
        geo->DrawArgs[name] = submesh;
    }
    _geometries[geo->Name] = std::move(geo);
}

void SkinnedAnimation::BuildSkullGeometry()
{
}

void SkinnedAnimation::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePso;

    ZeroMemory(&opaquePso, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePso.InputLayout = { _inputLayout.data(), (UINT)_inputLayout.size() };
    opaquePso.pRootSignature = _rootSignature.Get();
    opaquePso.VS =
    {
        reinterpret_cast<BYTE*>(_shaders["standardVS"]->GetBufferPointer()),
        _shaders["standardVS"]->GetBufferSize()
    };
    opaquePso.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["opaquePS"]->GetBufferPointer()),
        _shaders["opaquePS"]->GetBufferSize()
    };
    opaquePso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    opaquePso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePso.SampleMask = UINT_MAX;
    opaquePso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePso.NumRenderTargets = 1;
    opaquePso.RTVFormats[0] = _backBufferFormat;
    opaquePso.SampleDesc.Count = _4xMsaa ? 4 : 1;
    opaquePso.SampleDesc.Quality = _4xMsaa ? (_4xMsaaQuality - 1) : 0;
    opaquePso.DSVFormat = _dsvFormat;

    ThrowIfFailed(_device->CreateGraphicsPipelineState(&opaquePso, IID_PPV_ARGS(&_PSOs["opaque"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePso;
    skinnedOpaquePsoDesc.InputLayout = { _skinnedInputLayout.data(), (UINT)_skinnedInputLayout.size() };
    skinnedOpaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(_shaders["skinnedVS"]->GetBufferPointer()),
        _shaders["skinnedVS"]->GetBufferSize()
    };
    skinnedOpaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["opaquePS"]->GetBufferPointer()),
        _shaders["opaquePS"]->GetBufferSize()
    };
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc, IID_PPV_ARGS(&_PSOs["skinnedOpaque"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = opaquePso;
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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedSmapPsoDesc = smapPsoDesc;
    skinnedSmapPsoDesc.InputLayout = { _skinnedInputLayout.data(), (UINT)_skinnedInputLayout.size() };
    skinnedSmapPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(_shaders["skinnedShadowVS"]->GetBufferPointer()),
        _shaders["skinnedShadowVS"]->GetBufferSize()
    };
    skinnedSmapPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["shadowOpaquePS"]->GetBufferPointer()),
        _shaders["shadowOpaquePS"]->GetBufferSize()
    };
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&skinnedSmapPsoDesc, IID_PPV_ARGS(&_PSOs["skinnedShadow_opaque"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = opaquePso;
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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalsPsoDesc = opaquePso;
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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedDrawNormalsPsoDesc = drawNormalsPsoDesc;
    skinnedDrawNormalsPsoDesc.InputLayout = { _skinnedInputLayout.data(), (UINT)_skinnedInputLayout.size() };
    skinnedDrawNormalsPsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(_shaders["skinnedDrawNormalsVS"]->GetBufferPointer()),
        _shaders["skinnedDrawNormalsVS"]->GetBufferSize()
    };
    skinnedDrawNormalsPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["drawNormalsPS"]->GetBufferPointer()),
        _shaders["drawNormalsPS"]->GetBufferSize()
    };
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&skinnedDrawNormalsPsoDesc, IID_PPV_ARGS(&_PSOs["skinnedDrawNormals"])));


    D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = opaquePso;
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


    D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePso;
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

void SkinnedAnimation::BuildFrameResources()
{
    for (int i = 0; i < FrameResource::NumFrameResources; i++)
        _frameResources.push_back(std::make_unique<FrameResource>(_device.Get(), 2, (UINT)_allRenderItems.size(), 1, (UINT)_materials.size()));
}

void SkinnedAnimation::BuildMaterials()
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

    auto sky = std::make_unique<Material>();
    sky->Name = "sky";
    sky->MatCBIndex = 3;
    sky->DiffuseSrvHeapIndex = 6;
    sky->NormalSrvHeapIndex = 7;
    sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    sky->Roughness = 1.0f;

    _materials["bricks0"] = move(bricks0);
    _materials["tile0"] = move(tile0);
    _materials["mirror0"] = move(mirror0);
    _materials["sky"] = move(sky);

    UINT matCBIndex = 4;
    UINT srvHeapIndex = _skinnedSrvHeapStart;
    for (UINT i = 0; i < _skinnedMats.size(); i++)
    {
        auto mat = std::make_unique<Material>();
        mat->Name = _skinnedMats[i].Name;
        mat->MatCBIndex = matCBIndex++;
        mat->DiffuseSrvHeapIndex = srvHeapIndex++;
        mat->NormalSrvHeapIndex = srvHeapIndex++;
        mat->DiffuseAlbedo = _skinnedMats[i].DiffuseAlbedo;
        mat->FresnelR0 = _skinnedMats[i].FresnelR0;
        mat->Roughness = _skinnedMats[i].Roughness;

        _materials[mat->Name] = std::move(mat);
    }
}

void SkinnedAnimation::BuildRenderItems()
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
    XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
    boxRitem->ObjCBIndex = 2;
    boxRitem->Mat = _materials["bricks0"].get();
    boxRitem->Geo = _geometries["shapeGeo"].get();
    boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
    boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
    _renderItemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());
    _allRenderItems.push_back(move(boxRitem));
    
    auto gridRitem = std::make_unique<RenderItem>();
    gridRitem->Model = MathHelper::Identity4x4();
    XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
    gridRitem->ObjCBIndex = 3;
    gridRitem->Mat = _materials["tile0"].get();
    gridRitem->Geo = _geometries["shapeGeo"].get();
    gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
    _renderItemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
    _allRenderItems.push_back(move(gridRitem));

    XMMATRIX brickTexTransform = XMMatrixScaling(1.5f, 2.0f, 1.0f);
    UINT objCBIndex = 4;
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
    for (UINT i = 0; i < _skinnedMats.size(); i++)
    {
        std::string submeshName = "sm_" + std::to_string(i);

        auto ritem = std::make_unique<RenderItem>();

        XMMATRIX modelScale = XMMatrixScaling(0.05f, 0.05f, -0.05f);
        XMMATRIX modelRot = XMMatrixRotationY(MathHelper::Pi);
        XMMATRIX modelOffset = XMMatrixTranslation(0.0f, 0.0f, -5.0f);
        XMStoreFloat4x4(&ritem->Model, modelScale*modelRot*modelOffset);

        ritem->TexTransform = MathHelper::Identity4x4();
        ritem->ObjCBIndex = objCBIndex++;
        ritem->Mat = _materials[_skinnedMats[i].Name].get();
        ritem->Geo = _geometries[_skinnedModelFilename].get();
        ritem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        ritem->IndexCount = ritem->Geo->DrawArgs[submeshName].IndexCount;
        ritem->StartIndexLocation = ritem->Geo->DrawArgs[submeshName].StartIndexLocation;
        ritem->BaseVertexLocation = ritem->Geo->DrawArgs[submeshName].BaseVertexLocation;

        ritem->SkinnedCBIndex = 0;
        ritem->SkinnedModelInst = _skinnedModelInst.get();

        _renderItemLayer[(int)RenderLayer::SkinnedOpaque].push_back(ritem.get());
        _allRenderItems.push_back(std::move(ritem));
    }
}

void SkinnedAnimation::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems)
{
    UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(FrameResource::ObjectConstants));
    UINT skinnedCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(SkinnedAnimFrameResource::SkinnedConstants));
    
    auto objectCB = _currFrameResource->ObjectCB->Resource();
    auto skinnedCB = _currFrameResource->SkinnedCB->Resource();

    for (size_t i = 0; i < renderItems.size(); i++)
    {
        auto ri = renderItems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        if (ri->SkinnedModelInst != nullptr)
        {
            D3D12_GPU_VIRTUAL_ADDRESS skinnedCBAddress = skinnedCB->GetGPUVirtualAddress();
            cmdList->SetGraphicsRootConstantBufferView(1, skinnedCBAddress);
        }
        else
        {
            cmdList->SetGraphicsRootConstantBufferView(1, 0);
        }

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void SkinnedAnimation::DrawSceneToShadowMap()
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
    _commandList->SetGraphicsRootConstantBufferView(2, passCBAdderss);

    _commandList->SetPipelineState(_PSOs["shadow_opaque"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::Opaque]);

    _commandList->SetPipelineState(_PSOs["skinnedShadow_opaque"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::SkinnedOpaque]);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_shadowMap->Resource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SkinnedAnimation::DrawNormalsAndDepth()
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
    _commandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    _commandList->SetPipelineState(_PSOs["drawNormals"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::Opaque]);

    _commandList->SetPipelineState(_PSOs["skinnedDrawNormals"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::SkinnedOpaque]);


    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(normalMap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedAnimation::GetCpuSrv(int index) const
{
    auto srv = CD3DX12_CPU_DESCRIPTOR_HANDLE(_srvHeap->GetCPUDescriptorHandleForHeapStart());
    srv.Offset(index, _cbvSrvUavDescriptorSize);
    return srv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SkinnedAnimation::GetGpuSrv(int index) const
{
    auto srv = CD3DX12_GPU_DESCRIPTOR_HANDLE(_srvHeap->GetGPUDescriptorHandleForHeapStart());
    srv.Offset(index, _cbvSrvUavDescriptorSize);
    return srv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedAnimation::GetDsv(int index) const
{
    auto dsv = CD3DX12_CPU_DESCRIPTOR_HANDLE(_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    dsv.Offset(index, _dsvDescriptorSize);
    return dsv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedAnimation::GetRtv(int index) const
{
    auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    rtv.Offset(index, _rtvDescriptorSize);
    return rtv;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> SkinnedAnimation::GetStaticSamplers()
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

    return{
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp,
        shadow
    };
}
