#include "Picking.h"
#include "../DynamicIndexing/DynamicIndexingFrameResource.h"
#include "../../../Core/GeometryGenerator.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;
using Vertex = DynamicIndexingFrameResource::Vertex;
using FrameResource = DynamicIndexingFrameResource;
using RenderLayer = RenderItem::RenderLayer;

Picking::Picking(HINSTANCE hInstance) : Application(hInstance)
{
}

bool Picking::Init()
{
    if (!Application::Init())
        return false;
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    _camera.SetPosition(0.0f, 2.0f, -15.0f);

    LoadTextures();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShaderAndInputLayout();
    BuildCarGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(1, cmdLists);
    FlushCommandQueue();

    return true;
}

Picking::~Picking()
{
    if (_device != nullptr)
        FlushCommandQueue();
}

LRESULT Picking::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int Picking::Run()
{
    return Application::Run();
}

void Picking::OnResize()
{
    Application::OnResize();
    _camera.SetFrustum(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void Picking::Update(const GameTimer& timer)
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
    UpdateObjectCBs(timer);
    UpdateMaterialBuffer(timer);
    UpdateMainPassCB(timer);
}

void Picking::Draw(const GameTimer& timer)
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

    ID3D12DescriptorHeap* descriptorHeaps[] = { _srvHeap.Get() };
    _commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    _commandList->SetGraphicsRootSignature(_rootSignature.Get());

    auto passCB = _currFrameResource->PassCB->Resource();
    _commandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

    auto matBuf = _currFrameResource->MaterialBuffer->Resource();
    _commandList->SetGraphicsRootShaderResourceView(2, matBuf->GetGPUVirtualAddress());

    _commandList->SetGraphicsRootDescriptorTable(3, _srvHeap->GetGPUDescriptorHandleForHeapStart());

    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::Opaque]);

    _commandList->SetPipelineState(_PSOs["highlight"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderLayer::Highlight]);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* cmdsLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(_swapChain->Present(0, 0));
    _currBackBuffer = (_currBackBuffer + 1) % _swapChainBufferCount;

    _currFrameResource->Fence = ++_currentFence;
    _commandQueue->Signal(_fence.Get(), _currentFence);
}

void Picking::OnMouseDown(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        _lastMousePos.x = x;
        _lastMousePos.y = y;

        SetCapture(_hMainWindow);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        Pick(x, y);
    }
}

void Picking::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void Picking::OnMouseMove(WPARAM btnState, int x, int y)
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

void Picking::OnKeyboardInput(const GameTimer& timer)
{
    const float dt = timer.DeltaTime();

    if (GetAsyncKeyState('W') & 0x8000)
        _camera.Walk(10.0f*dt);

    if (GetAsyncKeyState('S') & 0x8000)
        _camera.Walk(-10.0f*dt);

    if (GetAsyncKeyState('A') & 0x8000)
        _camera.Strafe(-10.0f*dt);

    if (GetAsyncKeyState('D') & 0x8000)
        _camera.Strafe(10.0f*dt);

    _camera.UpdateViewMatrix();
}

void Picking::AnimateMaterials(const GameTimer& timer)
{
}

void Picking::UpdateObjectCBs(const GameTimer& timer)
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

void Picking::UpdateMaterialBuffer(const GameTimer& timer)
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

            currMaterialBuffer->CopyData(mat->MatCBIndex, matData);
            mat->NumFramesDirty--;
        }
    }
}

void Picking::UpdateMainPassCB(const GameTimer& timer)
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
    _mainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

    _mainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    _mainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
    _mainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    _mainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
    _mainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    _mainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

    auto currPassCB = _currFrameResource->PassCB.get();
    currPassCB->CopyData(0, _mainPassCB);
}

void Picking::LoadTextures()
{
    auto defaultDiffuseTex = std::make_unique<Texture>();
    defaultDiffuseTex->Name = "defaultDiffuseTex";
    defaultDiffuseTex->Filename = L"Textures/white1x1.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), defaultDiffuseTex->Filename.c_str(), defaultDiffuseTex->Resource, defaultDiffuseTex->UploadHeap));

    _textures[defaultDiffuseTex->Name] = move(defaultDiffuseTex);
}

void Picking::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0, 0);
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];
    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);
    slotRootParameter[2].InitAsShaderResourceView(0, 1);
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

    auto staticSamplers = FrameResourceUnfogged::GetStaticSamplers();
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(_rootSignature.GetAddressOf())));
}

void Picking::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_srvHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(_srvHeap->GetCPUDescriptorHandleForHeapStart());
    auto defaultDiffuseTex = _textures["defaultDiffuseTex"]->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = defaultDiffuseTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = defaultDiffuseTex->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    _device->CreateShaderResourceView(defaultDiffuseTex.Get(), &srvDesc, hDescriptor);
}

void Picking::BuildShaderAndInputLayout()
{
    _shaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\DynamicIndexing.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["opaquePS"] = D3DUtil::CompileShader(L"Shaders\\DynamicIndexing.hlsl", nullptr, "frag", "ps_5_1");

    _inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void Picking::BuildCarGeometry()
{
    std::ifstream fin("Models/car.txt");
    if (!fin)
    {
        MessageBox(0, L"Models/car.txt not found", 0, 0);
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
        vertices[i].Uv = { 0.0f, 0.0f };
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
        fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
    fin.close();

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(int32_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "carGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

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
    submesh.Bounds = bounds;
    geo->DrawArgs["car"] = submesh;
    _geometries[geo->Name] = move(geo);
}

void Picking::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { _inputLayout.data(), (UINT)_inputLayout.size() };
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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC highlightPsoDesc = opaquePsoDesc;
    highlightPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

    D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
    transparencyBlendDesc.BlendEnable = true;
    transparencyBlendDesc.LogicOpEnable = false;
    transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    highlightPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&highlightPsoDesc, IID_PPV_ARGS(&_PSOs["highlight"])));
}

void Picking::BuildFrameResources()
{
    for (int i = 0; i < FrameResource::NumFrameResources; i++)
    {
        _frameResources.push_back(std::make_unique<FrameResource>(_device.Get(), 1, (UINT)_allRenderItems.size(), (UINT)_materials.size()));
    }
}

void Picking::BuildMaterials()
{
    auto gray0 = std::make_unique<Material>();
    gray0->Name = "gray0";
    gray0->MatCBIndex = 0;
    gray0->DiffuseSrvHeapIndex = 0;
    gray0->DiffuseAlbedo = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
    gray0->FresnelR0 = XMFLOAT3(0.04f, 0.04f, 0.04f);
    gray0->Roughness = 0.0f;

    auto highlight0 = std::make_unique<Material>();
    highlight0->Name = "highlight";
    highlight0->MatCBIndex = 1;
    highlight0->DiffuseSrvHeapIndex = 0;
    highlight0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.6f);
    highlight0->FresnelR0 = XMFLOAT3(0.06f, 0.06f, 0.06f);
    highlight0->Roughness = 0.0f;

    _materials["gray0"] = move(gray0);
    _materials["highlight0"] = move(highlight0);
}

void Picking::BuildRenderItems()
{
    auto carRenderItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&carRenderItem->Model, XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f));
    XMStoreFloat4x4(&carRenderItem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
    carRenderItem->ObjCBIndex = 0;
    carRenderItem->Mat = _materials["gray0"].get();
    carRenderItem->Geo = _geometries["carGeo"].get();
    carRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    carRenderItem->Bounds = carRenderItem->Geo->DrawArgs["car"].Bounds;
    carRenderItem->IndexCount = carRenderItem->Geo->DrawArgs["car"].IndexCount;
    carRenderItem->StartIndexLocation = carRenderItem->Geo->DrawArgs["car"].StartIndexLocation;
    carRenderItem->BaseVertexLocation = carRenderItem->Geo->DrawArgs["car"].BaseVertexLocation;
    _renderItemLayer[(int)RenderLayer::Opaque].push_back(carRenderItem.get());

    auto pickedRenderItem = std::make_unique<RenderItem>();
    pickedRenderItem->Model = MathHelper::Identity4x4();
    pickedRenderItem->TexTransform = MathHelper::Identity4x4();
    pickedRenderItem->ObjCBIndex = 1;
    pickedRenderItem->Mat = _materials["highlight0"].get();
    pickedRenderItem->Geo = _geometries["carGeo"].get();
    pickedRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    pickedRenderItem->Visible = false;
    pickedRenderItem->IndexCount = 0;
    pickedRenderItem->StartIndexLocation = 0;
    pickedRenderItem->BaseVertexLocation = 0;
    _renderItemLayer[(int)RenderLayer::Highlight].push_back(pickedRenderItem.get());

    _allRenderItems.push_back(move(carRenderItem));
    _allRenderItems.push_back(move(pickedRenderItem));
}

void Picking::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems)
{
    UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(FrameResource::ObjectConstants));


    auto objectCB = _currFrameResource->ObjectCB->Resource();

    for (size_t i = 0; i < renderItems.size(); i++)
    {
        auto ri = renderItems[i];

        if (ri->Visible == false)
            continue;

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void Picking::Pick(int sx, int sy)
{
    XMFLOAT4X4 P = _camera.GetProj4x4f();

    float vx = (+2.0f * sx / _clientWidth - 1.0) / P(0, 0);
    float vy = (-2.0f * sy / _clientHeight + 1.0) / P(1, 1);

    XMVECTOR rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

    XMMATRIX V = _camera.GetView();
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(V), V);
    _pickedRenderItem->Visible = false;

    for (auto ri : _renderItemLayer[(int)RenderLayer::Opaque])
    {
        auto geo = ri->Geo;
        if (ri->Visible == false)
            continue;

        XMMATRIX M = XMLoadFloat4x4(&ri->Model);
        XMMATRIX invModel = XMMatrixInverse(&XMMatrixDeterminant(M), M);
        XMMATRIX toLocal = XMMatrixMultiply(invView, invModel);

        rayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
        rayDir = XMVector3TransformNormal(rayDir, toLocal);

        rayDir = XMVector3Normalize(rayDir);

        float tmin = 0.0f;
        if (ri->Bounds.Intersects(rayOrigin, rayDir, tmin))
        {
            auto vertices = (Vertex*)geo->VertexBufferCPU->GetBufferPointer();
            auto indices = (std::uint32_t*)geo->IndexBufferCPU->GetBufferPointer();
            UINT triCount = ri->IndexCount / 3;
            tmin = MathHelper::Infinity;
            for (UINT i = 0; i < triCount; i++)
            {
                UINT i0 = indices[i * 3 + 0];
                UINT i1 = indices[i * 3 + 1];
                UINT i2 = indices[i * 3 + 2];

                XMVECTOR v0 = XMLoadFloat3(&vertices[i0].Pos);
                XMVECTOR v1 = XMLoadFloat3(&vertices[i1].Pos);
                XMVECTOR v2 = XMLoadFloat3(&vertices[i2].Pos);
                    
                float t = 0.0f;
                if (TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, t))
                {
                    if (t < tmin)
                    {
                        tmin = t;
                        UINT pickedTriangle = i;

                        _pickedRenderItem->Visible = true;
                        _pickedRenderItem->IndexCount = 3;
                        _pickedRenderItem->BaseVertexLocation = 0;

                        _pickedRenderItem->Model = ri->Model;
                        _pickedRenderItem->NumFramesDirty = FrameResource::NumFrameResources;

                        _pickedRenderItem->StartIndexLocation = 3 * pickedTriangle;
                    }
                }
            }
        }
    }
}
