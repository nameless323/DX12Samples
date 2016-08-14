#include "Instancing.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;
using Vertex = InstancingFrameResource::Vertex;
using FrameResource = InstancingFrameResource;
using RenderItem = InstancingRenderItem;

Instancing::Instancing(HINSTANCE hInstance) : Application(hInstance)
{
}

bool Instancing::Init()
{
    if (!Application::Init())
        return false;

    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    _camera.SetPosition(0.0f, 2.0f, -15.0f);

    LoadTextures();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShaderAndInputLayout();
    BuildSkullGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdsLists[] = {_commandList.Get()};
    _commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();
    return true;
}

Instancing::~Instancing()
{
    if (_device != nullptr)
        FlushCommandQueue();
}

LRESULT Instancing::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int Instancing::Run()
{
    return Application::Run();
}

void Instancing::OnResize()
{
    Application::OnResize();
    _camera.SetFrustum(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

    BoundingFrustum::CreateFromMatrix(_camFrustum, _camera.GetProj());
}

void Instancing::Update(const GameTimer& timer)
{
    OnKeyboardInput(timer);

    _currentFrameResourceIndex = (_currentFrameResourceIndex + 1) % FrameResource::NumFrameResources;
    _currFrameResource = _frameResources[_currentFrameResourceIndex].get();

    if (_currFrameResource->Fence != 0 && _fence->GetCompletedValue() < _currFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(_fence->SetEventOnCompletion(_currFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
    AnimateMaterials(timer);
    UpdateInstanceData(timer);
    UpdateMaterialBuffer(timer);
    UpdateMainPassCB(timer);
}

void Instancing::Draw(const GameTimer& timer)
{
    auto cmdListAlloc = _currFrameResource->CmdListAlloc;

    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(_commandList->Reset(cmdListAlloc.Get(), _PSOs["opaque"].Get()));

    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    _commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    _commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    _commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = {_srvHeap.Get()};
    _commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    _commandList->SetGraphicsRootSignature(_rootSignature.Get());

    auto matBuffer = _currFrameResource->MaterialBuffer->Resource();
    _commandList->SetGraphicsRootShaderResourceView(1, matBuffer->GetGPUVirtualAddress());

    auto passCB = _currFrameResource->PassCB->Resource();
    _commandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    _commandList->SetGraphicsRootDescriptorTable(3, _srvHeap->GetGPUDescriptorHandleForHeapStart());

    DrawRenderItems(_commandList.Get(), _opaqueRenderItems);
    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* cmdLists[] = {_commandList.Get()};
    _commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    ThrowIfFailed(_swapChain->Present(0, 0));
    _currBackBuffer = (_currBackBuffer + 1) % _swapChainBufferCount;

    _currFrameResource->Fence = ++_currentFence;
    _commandQueue->Signal(_fence.Get(), _currentFence);
}

void Instancing::OnMouseDown(WPARAM btnState, int x, int y)
{
    _lastMousePos.x = x;
    _lastMousePos.y = y;
    SetCapture(_hMainWindow);
}

void Instancing::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void Instancing::OnMouseMove(WPARAM btnState, int x, int y)
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

void Instancing::OnKeyboardInput(const GameTimer& timer)
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

    if (GetAsyncKeyState('1') & 0x8000)
        _frustumCullingEnabled = true;

    if (GetAsyncKeyState('2') & 0x8000)
        _frustumCullingEnabled = false;

    _camera.UpdateViewMatrix();
}

void Instancing::AnimateMaterials(const GameTimer& timer)
{
}

void Instancing::UpdateInstanceData(const GameTimer& timer)
{
    XMMATRIX view = _camera.GetView();
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

    auto currInstanceBuffer = _currFrameResource->InstanceBuffer.get();
    for (auto& e : _allRenderItems)
    {
        const auto& instanceData = e->Instances;
        int visibleInstanceCount = 0;

        for (UINT i = 0; i < (UINT)instanceData.size(); i++)
        {
            XMMATRIX model = XMLoadFloat4x4(&instanceData[i].Model);
            XMMATRIX texTransform = XMLoadFloat4x4(&instanceData[i].TexTransform);

            XMMATRIX invModel = XMMatrixInverse(&XMMatrixDeterminant(model), model);

            XMMATRIX viewToLocal = XMMatrixMultiply(invView, invModel);
            BoundingFrustum localSpaceFrustum;
            _camFrustum.Transform(localSpaceFrustum, viewToLocal);

            if (_frustumCullingEnabled == false || localSpaceFrustum.Contains(e->Bounds) != DISJOINT)
            {
                FrameResource::InstanceData data;
                XMStoreFloat4x4(&data.Model, XMMatrixTranspose(model));
                XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(texTransform));
                data.MaterialIndex = instanceData[i].MaterialIndex;

                currInstanceBuffer->CopyData(visibleInstanceCount++, data);
            }
        }
        e->InstanceCount = visibleInstanceCount;

        std::wostringstream outs;
        outs.precision(6);
        outs << L"Instancing and culling" << L"   " << e->InstanceCount << L" object visible out of " << e->Instances.size();
        _mainWindowCaption = outs.str();
    }
}

void Instancing::UpdateMaterialBuffer(const GameTimer& timer)
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
            XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));
            matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
            currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

            mat->NumFramesDirty--;
        }
    }
}

void Instancing::UpdateMainPassCB(const GameTimer& timer)
{
    XMMATRIX view = _camera.GetView();
    XMMATRIX proj = _camera.GetProj();

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
    _mainPassCB.EyePosW = _camera.GetPosition3f();
    _mainPassCB.RenderTargetSize = XMFLOAT2((float)_clientWidth, (float)_clientHeight);
    _mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / _clientWidth, 1.0f / _clientHeight);
    _mainPassCB.NearZ = 1.0f;
    _mainPassCB.FarZ = 1000.0f;
    _mainPassCB.TotalTime = timer.TotalTime();
    _mainPassCB.DeltaTime = timer.DeltaTime();
    _mainPassCB.AmbientLight = {0.25f, 0.25f, 0.35f, 1.0f};

    _mainPassCB.Lights[0].Direction = {0.57735f, -0.57735f, 0.57735f};
    _mainPassCB.Lights[0].Strength = {0.8f, 0.8f, 0.8f};
    _mainPassCB.Lights[1].Direction = {-0.57735f, -0.57735f, 0.57735f};
    _mainPassCB.Lights[1].Strength = {0.4f, 0.4f, 0.4f};
    _mainPassCB.Lights[2].Direction = {0.0f, -0.707f, -0.707f};
    _mainPassCB.Lights[2].Strength = {0.2f, 0.2f, 0.2f};

    auto currPassCB = _currFrameResource->PassCB.get();
    currPassCB->CopyData(0, _mainPassCB);
}

void Instancing::LoadTextures()
{
    auto brickTex = std::make_unique<Texture>();
    brickTex->Name = "bricksTex";
    brickTex->Filename = L"Textures/bricks.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), brickTex->Filename.c_str(), brickTex->Resource, brickTex->UploadHeap));

    auto stoneTex = std::make_unique<Texture>();
    stoneTex->Name = "stoneTex";
    stoneTex->Filename = L"Textures/stone.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), stoneTex->Filename.c_str(), stoneTex->Resource, stoneTex->UploadHeap));

    auto tileTex = std::make_unique<Texture>();
    tileTex->Name = "tileTex";
    tileTex->Filename = L"Textures/tile.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), tileTex->Filename.c_str(), tileTex->Resource, tileTex->UploadHeap));

    auto crateTex = std::make_unique<Texture>();
    crateTex->Name = "crateTex";
    crateTex->Filename = L"Textures/WoodCrate01.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), crateTex->Filename.c_str(), crateTex->Resource, crateTex->UploadHeap));

    auto iceTex = std::make_unique<Texture>();
    iceTex->Name = "iceTex";
    iceTex->Filename = L"Textures/ice.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), iceTex->Filename.c_str(), iceTex->Resource, iceTex->UploadHeap));

    auto grassTex = std::make_unique<Texture>();
    grassTex->Name = "grassTex";
    grassTex->Filename = L"Textures/grass.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), grassTex->Filename.c_str(), grassTex->Resource, grassTex->UploadHeap));

    auto defaultTex = std::make_unique<Texture>();
    defaultTex->Name = "defaultTex";
    defaultTex->Filename = L"Textures/white1x1.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), defaultTex->Filename.c_str(), defaultTex->Resource, defaultTex->UploadHeap));

    _textures[brickTex->Name] = move(brickTex);
    _textures[stoneTex->Name] = move(stoneTex);
    _textures[tileTex->Name] = move(tileTex);
    _textures[crateTex->Name] = move(crateTex);
    _textures[iceTex->Name] = move(iceTex);
    _textures[grassTex->Name] = move(grassTex);
    _textures[defaultTex->Name] = move(defaultTex);
}

void Instancing::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7, 0, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    slotRootParameter[0].InitAsShaderResourceView(0, 1);
    slotRootParameter[1].InitAsShaderResourceView(1, 1);
    slotRootParameter[2].InitAsConstantBufferView(0);
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = FrameResource::GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(_rootSignature.GetAddressOf())));
}

void Instancing::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 7;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_srvHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(_srvHeap->GetCPUDescriptorHandleForHeapStart());
    auto bricksTex = _textures["bricksTex"]->Resource;
    auto stoneTex = _textures["stoneTex"]->Resource;
    auto tileTex = _textures["tileTex"]->Resource;
    auto crateTex = _textures["crateTex"]->Resource;
    auto iceTex = _textures["iceTex"]->Resource;
    auto grassTex = _textures["grassTex"]->Resource;
    auto defaultTex = _textures["defaultTex"]->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = bricksTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    _device->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, _cbvSrvUavDescriptorSize);

    srvDesc.Format = stoneTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = stoneTex->GetDesc().MipLevels;
    _device->CreateShaderResourceView(stoneTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, _cbvSrvUavDescriptorSize);
    srvDesc.Format = tileTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = tileTex->GetDesc().MipLevels;
    _device->CreateShaderResourceView(tileTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, _cbvSrvUavDescriptorSize);
    srvDesc.Format = crateTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = crateTex->GetDesc().MipLevels;
    _device->CreateShaderResourceView(crateTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, _cbvSrvUavDescriptorSize);
    srvDesc.Format = iceTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = iceTex->GetDesc().MipLevels;
    _device->CreateShaderResourceView(iceTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, _cbvSrvUavDescriptorSize);
    srvDesc.Format = grassTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = grassTex->GetDesc().MipLevels;
    _device->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, _cbvSrvUavDescriptorSize);
    srvDesc.Format = defaultTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = defaultTex->GetDesc().MipLevels;
    _device->CreateShaderResourceView(defaultTex.Get(), &srvDesc, hDescriptor);
}

void Instancing::BuildShaderAndInputLayout()
{
    _shaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\Instancing.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["opaquePS"] = D3DUtil::CompileShader(L"Shaders\\Instancing.hlsl", nullptr, "frag", "ps_5_1");

    _inputLayout =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        };
}

void Instancing::BuildSkullGeometry()
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

        XMFLOAT3 spherePos;
        XMStoreFloat3(&spherePos, XMVector3Normalize(P));

        float theta = atan2f(spherePos.z, spherePos.x);

        if (theta < 0.0f)
            theta += XM_2PI;

        float phi = acosf(spherePos.y);

        float u = theta / (2.0f * XM_PI);
        float v = phi / XM_PI;

        vertices[i].Uv = {u, v};

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

void Instancing::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = {_inputLayout.data(), (UINT)_inputLayout.size()};
    opaquePsoDesc.pRootSignature = _rootSignature.Get();
    opaquePsoDesc.VS =
        {
            reinterpret_cast<BYTE*>(_shaders["standardVS"]->GetBufferPointer()),
            _shaders["standardVS"]->GetBufferSize()
        };
    opaquePsoDesc.PS =
        {
            reinterpret_cast<BYTE*>(_shaders["opaquePS"]->GetBufferPointer()),
            _shaders["opaquePS"]->GetBufferSize()
        };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = _backBufferFormat;
    opaquePsoDesc.SampleDesc.Count = _4xMsaa ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = _4xMsaa ? (_4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = _dsvFormat;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&_PSOs["opaque"])));
}

void Instancing::BuildFrameResources()
{
    for (int i = 0; i < FrameResource::NumFrameResources; i++)
    {
        _frameResources.push_back(std::make_unique<FrameResource>(_device.Get(), 1, _numInstances, (UINT)_materials.size()));
    }
}

void Instancing::BuildMaterials()
{
    auto bricks0 = std::make_unique<Material>();
    bricks0->Name = "bricks0";
    bricks0->MatCBIndex = 0;
    bricks0->DiffuseSrvHeapIndex = 0;
    bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    bricks0->Roughness = 0.1f;

    auto stone0 = std::make_unique<Material>();
    stone0->Name = "stone0";
    stone0->MatCBIndex = 1;
    stone0->DiffuseSrvHeapIndex = 1;
    stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    stone0->Roughness = 0.3f;

    auto tile0 = std::make_unique<Material>();
    tile0->Name = "tile0";
    tile0->MatCBIndex = 2;
    tile0->DiffuseSrvHeapIndex = 2;
    tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    tile0->Roughness = 0.3f;

    auto crate0 = std::make_unique<Material>();
    crate0->Name = "crate0";
    crate0->MatCBIndex = 3;
    crate0->DiffuseSrvHeapIndex = 3;
    crate0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    crate0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    crate0->Roughness = 0.3f;

    auto ice0 = std::make_unique<Material>();
    ice0->Name = "ice0";
    ice0->MatCBIndex = 4;
    ice0->DiffuseSrvHeapIndex = 4;
    ice0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    ice0->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    ice0->Roughness = 0.0f;

    auto grass0 = std::make_unique<Material>();
    grass0->Name = "grass0";
    grass0->MatCBIndex = 5;
    grass0->DiffuseSrvHeapIndex = 5;
    grass0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    grass0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    grass0->Roughness = 0.2f;

    auto skullMat = std::make_unique<Material>();
    skullMat->Name = "skullMat";
    skullMat->MatCBIndex = 6;
    skullMat->DiffuseSrvHeapIndex = 6;
    skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    skullMat->Roughness = 0.5f;

    _materials["bricks0"] = move(bricks0);
    _materials["stone0"] = move(stone0);
    _materials["tile0"] = move(tile0);
    _materials["crate0"] = move(crate0);
    _materials["ice0"] = move(ice0);
    _materials["grass0"] = move(grass0);
    _materials["skullMat"] = move(skullMat);
}

void Instancing::BuildRenderItems()
{
    auto skullRenderItem = std::make_unique<RenderItem>();
    skullRenderItem->Model = MathHelper::Identity4x4();
    skullRenderItem->TexTransform = MathHelper::Identity4x4();
    skullRenderItem->ObjCBIndex = 0;
    skullRenderItem->Mat = _materials["tile0"].get();
    skullRenderItem->Geo = _geometries["skullGeo"].get();
    skullRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skullRenderItem->InstanceCount = 0;
    skullRenderItem->IndexCount = skullRenderItem->Geo->DrawArgs["skull"].IndexCount;
    skullRenderItem->StartIndexLocation = skullRenderItem->Geo->DrawArgs["skull"].StartIndexLocation;
    skullRenderItem->BaseVertexLocation = skullRenderItem->Geo->DrawArgs["skull"].BaseVertexLocation;
    skullRenderItem->Bounds = skullRenderItem->Geo->DrawArgs["skull"].Bounds;

    const int n = cbrt(_numInstances);
    skullRenderItem->Instances.resize(n * n * n);

    float width = 150.0f;
    float height = 150.0f;
    float depth = 150.0f;

    float x = -0.5f * width;
    float y = -0.5f * height;
    float z = -0.5f * depth;
    float dx = width / (n - 1);
    float dy = height / (n - 1);
    float dz = depth / (n - 1);

    for (int k = 0; k < n; ++k)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int j = 0; j < n; ++j)
            {
                int index = k * n * n + i * n + j;
                skullRenderItem->Instances[index].Model = XMFLOAT4X4(
                    1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f,
                    x + j * dx, y + i * dy, z + k * dz, 1.0f);

                XMStoreFloat4x4(&skullRenderItem->Instances[index].TexTransform, XMMatrixScaling(2.0f, 2.0f, 1.0f));
                skullRenderItem->Instances[index].MaterialIndex = index % _materials.size();
            }
        }
    }
    _allRenderItems.push_back(move(skullRenderItem));
    for (auto& e : _allRenderItems)
        _opaqueRenderItems.push_back(e.get());
}

void Instancing::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems)
{
    for (size_t i = 0; i < renderItems.size(); i++)
    {
        auto ri = renderItems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        auto instanceBuffer = _currFrameResource->InstanceBuffer->Resource();
        _commandList->SetGraphicsRootShaderResourceView(0, instanceBuffer->GetGPUVirtualAddress());

        cmdList->DrawIndexedInstanced(ri->IndexCount, ri->InstanceCount, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}
