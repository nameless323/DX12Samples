#include "Blending.h"
#include "../../Common/RenderItem.h"
#include "../../../Core/GeometryGenerator.h"

namespace DX12Samples
{
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;

using Vertex = FrameResourceBlending::Vertex;
using PassConstants = FrameResourceBlending::PassConstants;
using ObjectConstants = FrameResourceBlending::ObjectConstants;

Blending::Blending(HINSTANCE hInstance) : Application(hInstance)
{
}

Blending::~Blending()
{
    if (_device != nullptr)
        FlushCommandQueue();
}

bool Blending::Init()
{
    if (!Application::Init())
        return false;

    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    _waves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

    LoadTextures();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShaderAndInputLayout();
    BuildLandGeometry();
    BuildWavesGeometry();
    BuildBoxGeometry();
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

LRESULT Blending::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int Blending::Run()
{
    return Application::Run();
}

void Blending::OnResize()
{
    Application::OnResize();

    XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&_proj, p);
}

void Blending::Update(const GameTimer& timer)
{
    OnKeyboardInput(timer);
    UpdateCamera(timer);

    _currentFrameResourceIndex = (_currentFrameResourceIndex + 1) % FrameResourceBlending::NumFrameResources;
    _currFrameResource = _frameResources[_currentFrameResourceIndex].get();

    if (_currFrameResource->Fence != 0 && _fence->GetCompletedValue() < _currFrameResource->Fence)
    {
        HANDLE eventHande = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(_fence->SetEventOnCompletion(_currFrameResource->Fence, eventHande));
        WaitForSingleObject(eventHande, INFINITE);
        CloseHandle(eventHande);
    }

    AnimateMaterials(timer);
    UpdateObjectCBs(timer);
    UpdateMaterialCBs(timer);
    UpdateMainPassCB(timer);
    UpdateWaves(timer);
}

void Blending::Draw(const GameTimer& timer)
{
    auto cmdListAlloc = _currFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(_commandList->Reset(cmdListAlloc.Get(), _PSOs["opaque"].Get()));
    _commandList->RSSetViewports(1, &_screenViewport);
    _commandList->RSSetScissorRects(1, &_scissorRect);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    _commandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&_mainPassCB.FogColor, 0, nullptr);
    _commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    _commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { _srvHeap.Get() };
    _commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    _commandList->SetGraphicsRootSignature(_rootSignature.Get());

    auto passCB = _currFrameResource->PassCB->Resource();
    _commandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderItem::RenderLayer::Opaque]);

    _commandList->SetPipelineState(_PSOs["alphaTested"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderItem::RenderLayer::AlphaTested]);

    _commandList->SetPipelineState(_PSOs["transparent"].Get());
    DrawRenderItems(_commandList.Get(), _renderItemLayer[(int)RenderItem::RenderLayer::Transparent]);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* cmdsLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(_swapChain->Present(0, 0));
    _currBackBuffer = (_currBackBuffer + 1) % _swapChainBufferCount;

    _currFrameResource->Fence = ++_currentFence;

    _commandQueue->Signal(_fence.Get(), _currentFence);
}

void Blending::OnMouseDown(WPARAM btnState, int x, int y)
{
    _lastMousePos.x = x;
    _lastMousePos.y = y;
    SetCapture(_hMainWindow);
}

void Blending::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void Blending::OnMouseMove(WPARAM btnState, int x, int y)
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

void Blending::OnKeyboardInput(const GameTimer& timer)
{
    float dt = timer.DeltaTime();
    if (GetAsyncKeyState('1') & 0x8000)
        _isWireframe = true;
    else
        _isWireframe = false;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
        _sunTheta -= 1.0f * dt;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        _sunTheta += 1.0f * dt;

    if (GetAsyncKeyState(VK_UP) & 0x8000)
        _sunPhi -= 1.0f * dt;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        _sunPhi += 1.0f * dt;

    _sunPhi = MathHelper::Clamp(_sunPhi, 0.1f, XM_PIDIV2);
}

void Blending::UpdateCamera(const GameTimer& timer)
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

void Blending::AnimateMaterials(const GameTimer& timer)
{
    auto waterMat = _materials["water"].get();

    float& tu = waterMat->MatTransform(3, 0);
    float& tv = waterMat->MatTransform(3, 1);

    tu += 0.1f * timer.DeltaTime();
    tv += 0.02f * timer.DeltaTime();

    if (tu >= 1.0f)
        tu -= 1.0f;
    if (tv >= 1.0f)
        tv -= 1.0f;

    waterMat->MatTransform(3, 0) = tu;
    waterMat->MatTransform(3, 1) = tv;

    waterMat->NumFramesDirty = FrameResourceBlending::NumFrameResources;
}

void Blending::UpdateObjectCBs(const GameTimer& timer)
{
    auto currObjCB = _currFrameResource->ObjectCB.get();
    for (auto& e : _allRenderItems)
    {
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX model = XMLoadFloat4x4(&e->Model);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.Model, XMMatrixTranspose(model));
            XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
            currObjCB->CopyData(e->ObjCBIndex, objConstants);
            e->NumFramesDirty--;
        }
    }
}

void Blending::UpdateMaterialCBs(const GameTimer& timer)
{
    auto currMaterialCB = _currFrameResource->MaterialCB.get();

    for (auto& e : _materials)
    {
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

            MaterialConstants matConstants;
            matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
            matConstants.FresnelR0 = mat->FresnelR0;
            matConstants.Roughness = mat->Roughness;
            XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);
            mat->NumFramesDirty--;
        }
    }
}

void Blending::UpdateMainPassCB(const GameTimer& timer)
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

    XMVECTOR lightDir = -MathHelper::SphericalToCartesian(1.0f, _sunTheta, _sunPhi);
    XMStoreFloat3(&_mainPassCB.Lights[0].Direction, lightDir);
    _mainPassCB.Lights[0].Strength = { 1.0f, 1.0f, 0.9f };
    _mainPassCB.Lights[1].Direction = { 0.57735f, -0.57735f, 0.57735f };
    _mainPassCB.Lights[1].Strength = { 0.9f, 0.9f, 0.9f };
    _mainPassCB.Lights[2].Direction = { -0.57735f, -0.57735f, 0.57735f };
    _mainPassCB.Lights[2].Strength = { 0.5f, 0.5f, 0.5f };

    auto currPassCB = _currFrameResource->PassCB.get();
    currPassCB->CopyData(0, _mainPassCB);
}

void Blending::UpdateWaves(const GameTimer& timer)
{
    static float base = 0.0f;
    if ((timer.TotalTime() - base) >= 0.25f)
    {
        base += 0.25f;
        int i = MathHelper::Rand(4, _waves->RowCount() - 5);
        int j = MathHelper::Rand(4, _waves->ColumnCount() - 5);

        float r = MathHelper::RandF(0.2f, 0.5f);

        _waves->Disturb(i, j, r);
    }
    _waves->Update(timer.DeltaTime());

    auto currWavesVB = _currFrameResource->WavesVB.get();
    for (int i = 0; i < _waves->VertexCount(); i++)
    {
        Vertex v;
        v.Pos = _waves->Position(i);
        v.Normal = _waves->Normal(i);

        v.TexC.x = 0.5f + v.Pos.x / _waves->Width();
        v.TexC.y = 0.5f + v.Pos.z / _waves->Depth();
        currWavesVB->CopyData(i, v);
    }
    _wavesRenderItem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void Blending::LoadTextures()
{
    auto grassTex = std::make_unique<Texture>();
    grassTex->Name = "grassTex";
    grassTex->Filename = L"Textures/grass.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), grassTex->Filename.c_str(), grassTex->Resource, grassTex->UploadHeap));

    auto waterTex = std::make_unique<Texture>();
    waterTex->Name = "waterTex";
    waterTex->Filename = L"Textures/water1.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), waterTex->Filename.c_str(), waterTex->Resource, waterTex->UploadHeap));

    auto fenceTex = std::make_unique<Texture>();
    fenceTex->Name = "fenceTex";
    fenceTex->Filename = L"Textures/WireFence.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), fenceTex->Filename.c_str(), fenceTex->Resource, fenceTex->UploadHeap));

    _textures[grassTex->Name] = move(grassTex);
    _textures[waterTex->Name] = move(waterTex);
    _textures[fenceTex->Name] = move(fenceTex);
}

void Blending::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

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

void Blending::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 3;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(_srvHeap.GetAddressOf())));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(_srvHeap->GetCPUDescriptorHandleForHeapStart());
    auto grassTex = _textures["grassTex"]->Resource;
    auto waterTex = _textures["waterTex"]->Resource;
    auto fenceTex = _textures["fenceTex"]->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = grassTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = -1;
    _device->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, _cbvSrvUavDescriptorSize);

    srvDesc.Format = waterTex->GetDesc().Format;
    _device->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, _cbvSrvUavDescriptorSize);

    srvDesc.Format = fenceTex->GetDesc().Format;
    _device->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);
}

void Blending::BuildShaderAndInputLayout()
{
    const D3D_SHADER_MACRO defines[] =
    {
        "FOG", "1",
        NULL, NULL
    };
    const D3D_SHADER_MACRO alphaTestDefines[] =
    {
        "FOG", "1",
        "ALPHA_TEST", "1",
        NULL, NULL
    };
    _shaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\Blending.hlsl", nullptr, "vert", "vs_5_0");
    _shaders["opaquePS"] = D3DUtil::CompileShader(L"Shaders\\Blending.hlsl", defines, "frag", "ps_5_0");
    _shaders["alphaTestedPS"] = D3DUtil::CompileShader(L"Shaders\\Blending.hlsl", alphaTestDefines, "frag", "ps_5_0");

    _inputLayout = 
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void Blending::BuildLandGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

    std::vector<FrameResourceUnfogged::Vertex> vertices(grid.Vertices.size());
    for (size_t i = 0; i < grid.Vertices.size(); i++)
    {
        auto& p = grid.Vertices[i].Position;
        vertices[i].Pos = p;
        vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
        vertices[i].Normal = GetHillsNormal(p.z, p.z);
        vertices[i].TexC = grid.Vertices[i].TexCoord;
    }
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(FrameResourceUnfogged::Vertex);
    std::vector<uint16_t> indices = grid.GetIndices16();
    const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "landGeo";

    geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(FrameResourceUnfogged::Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;
    _geometries["landGeo"] = move(geo);
}

void Blending::BuildWavesGeometry()
{
    std::vector<uint16_t> indices(3 * _waves->TriangleCount());
    assert(_waves->VertexCount() < 0x0000ffff);

    int m = _waves->RowCount();
    int n = _waves->ColumnCount();
    int k = 0;

    for (int i = 0; i < m - 1; ++i)
    {
        for (int j = 0; j < n - 1; ++j)
        {
            indices[k] = i*n + j;
            indices[k + 1] = i*n + j + 1;
            indices[k + 2] = (i + 1)*n + j;

            indices[k + 3] = (i + 1)*n + j;
            indices[k + 4] = i*n + j + 1;
            indices[k + 5] = (i + 1)*n + j + 1;

            k += 6;
        }
    }

    UINT vbByteSize = _waves->VertexCount() * sizeof(FrameResourceUnfogged::Vertex);
    UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "waterGeo";

    geo->VertexBufferGPU = nullptr;

    geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(FrameResourceUnfogged::Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;
    _geometries["waterGeo"] = move(geo);
}

void Blending::BuildBoxGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 3);

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = 0;
    boxSubmesh.BaseVertexLocation = 0;

    std::vector<FrameResourceUnfogged::Vertex> vertices(box.Vertices.size());

    for (size_t i = 0; i < box.Vertices.size(); i++)
    {
        vertices[i].Pos = box.Vertices[i].Position;
        vertices[i].Normal = box.Vertices[i].Normal;
        vertices[i].TexC = box.Vertices[i].TexCoord;
    }
    std::vector<uint16_t> indices = box.GetIndices16();
    const UINT vbByteSize = sizeof(FrameResourceUnfogged::Vertex) * (UINT)vertices.size();
    const UINT ibByteSize = sizeof(uint16_t) * (UINT)indices.size();

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "boxGeo";

    geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(FrameResourceUnfogged::Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["box"] = boxSubmesh;
    _geometries[geo->Name] = move(geo);
}

void Blending::BuildPSOs()
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

    D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

    D3D12_RENDER_TARGET_BLEND_DESC transparentBlendDesc;
    transparentBlendDesc.BlendEnable = true;
    transparentBlendDesc.LogicOpEnable = false;
    transparentBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    transparentBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    transparentBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    transparentBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    transparentBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    transparentBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    transparentBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    transparentBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    transparentPsoDesc.BlendState.RenderTarget[0] = transparentBlendDesc;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&_PSOs["transparent"])));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
    alphaTestedPsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(_shaders["alphaTestedPS"]->GetBufferPointer()),
        _shaders["alphaTestedPS"]->GetBufferSize()
    };
    alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&_PSOs["alphaTested"])));
}

void Blending::BuildFrameResources()
{
    for (int i = 0; i < FrameResourceBlending::NumFrameResources; i++)
        _frameResources.push_back(std::make_unique<FrameResourceBlending>(_device.Get(), 1, (UINT)_allRenderItems.size(), (UINT)_materials.size(), _waves->VertexCount()));
}

void Blending::BuildMaterials()
{
    auto grass = std::make_unique<Material>();
    grass->Name = "grass";
    grass->MatCBIndex = 0;
    grass->DiffuseSrvHeapIndex = 0;
    grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
    grass->Roughness = 0.125f;

    auto water = std::make_unique<Material>();
    water->Name = "water";
    water->MatCBIndex = 1;
    water->DiffuseSrvHeapIndex = 1;
    water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
    water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    water->Roughness = 0.0f;

    auto wireFence = std::make_unique<Material>();
    wireFence->Name = "wirefence";
    wireFence->MatCBIndex = 2;
    wireFence->DiffuseSrvHeapIndex = 2;
    wireFence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    wireFence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    wireFence->Roughness = 0.25f;

    _materials["grass"] = move(grass);
    _materials["water"] = move(water);
    _materials["wirefence"] = move(wireFence);
}

void Blending::BuildRenderItems()
{
    auto wavesRenderItem = std::make_unique<RenderItem>();
    wavesRenderItem->Model = MathHelper::Identity4x4();
    XMStoreFloat4x4(&wavesRenderItem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
    wavesRenderItem->ObjCBIndex = 0;
    wavesRenderItem->Mat = _materials["water"].get();
    wavesRenderItem->Geo = _geometries["waterGeo"].get();
    wavesRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    wavesRenderItem->IndexCount = wavesRenderItem->Geo->DrawArgs["grid"].IndexCount;
    wavesRenderItem->StartIndexLocation = wavesRenderItem->Geo->DrawArgs["grid"].StartIndexLocation;
    wavesRenderItem->BaseVertexLocation = wavesRenderItem->Geo->DrawArgs["grid"].BaseVertexLocation;

    _wavesRenderItem = wavesRenderItem.get();

    _renderItemLayer[(int)RenderItem::RenderLayer::Transparent].push_back(wavesRenderItem.get());

    auto gridRenderItem = std::make_unique<RenderItem>();
    gridRenderItem->Model = MathHelper::Identity4x4();
    XMStoreFloat4x4(&gridRenderItem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
    gridRenderItem->ObjCBIndex = 1;
    gridRenderItem->Mat = _materials["grass"].get();
    gridRenderItem->Geo = _geometries["landGeo"].get();
    gridRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRenderItem->IndexCount = gridRenderItem->Geo->DrawArgs["grid"].IndexCount;
    gridRenderItem->StartIndexLocation = gridRenderItem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRenderItem->BaseVertexLocation = gridRenderItem->Geo->DrawArgs["grid"].BaseVertexLocation;

    _renderItemLayer[(int)RenderItem::RenderLayer::Opaque].push_back(gridRenderItem.get());

    auto boxRenderItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&boxRenderItem->Model, XMMatrixTranslation(3.0f, 2.0f, -9.0f));
    boxRenderItem->ObjCBIndex = 2;
    boxRenderItem->Mat = _materials["wirefence"].get();
    boxRenderItem->Geo = _geometries["boxGeo"].get();
    boxRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRenderItem->IndexCount = boxRenderItem->Geo->DrawArgs["box"].IndexCount;
    boxRenderItem->StartIndexLocation = boxRenderItem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRenderItem->BaseVertexLocation = boxRenderItem->Geo->DrawArgs["box"].BaseVertexLocation;

    _renderItemLayer[(int)RenderItem::RenderLayer::AlphaTested].push_back(boxRenderItem.get());

    _allRenderItems.push_back(move(wavesRenderItem));
    _allRenderItems.push_back(move(gridRenderItem));
    _allRenderItems.push_back(move(boxRenderItem));
}

void Blending::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems)
{
    UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(FrameResourceUnfogged::ObjectConstants));
    UINT matCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

    auto objectCB = _currFrameResource->ObjectCB->Resource();
    auto materialCB = _currFrameResource->MaterialCB->Resource();

    for (size_t i = 0; i < renderItems.size(); i++)
    {
        auto ri = renderItems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        CD3DX12_GPU_DESCRIPTOR_HANDLE tex(_srvHeap->GetGPUDescriptorHandleForHeapStart());
        tex.Offset(ri->Mat->DiffuseSrvHeapIndex, _cbvSrvUavDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = materialCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

        cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

float Blending::GetHillsHeight(float x, float z) const
{
    return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 Blending::GetHillsNormal(float x, float z) const
{
    XMFLOAT3 n(
        -0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
        1.0f,
        -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));
    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
    XMStoreFloat3(&n, unitNormal);
    return n;
}
}