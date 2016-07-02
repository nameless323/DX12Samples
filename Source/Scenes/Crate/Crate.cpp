#include "Crate.h"
#include "../../../Core/GeometryGenerator.h"
#include "../../Common/FrameResource.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;

Crate::Crate(HINSTANCE hInstance) : Application(hInstance)
{
}

bool Crate::Init()
{
    if (!Application::Init())
        return false;

    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    LoadTextures();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShaderAndInputLayout();
    BuildShapeGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();
    return true;
}

Crate::~Crate()
{
    if (_device != nullptr)
        FlushCommandQueue();
}

LRESULT Crate::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int Crate::Run()
{
    return Application::Run();
}

void Crate::OnResize()
{
    Application::OnResize();
    XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&_proj, p);
}

void Crate::Update(const GameTimer& timer)
{
    OnKeyboardInput(timer);
    UpdateCamera(timer);

    _currentFrameResourceIndex = (_currentFrameResourceIndex + 1) % CrateFrameResource::NumFrameResources;
    _currFrameResource = _frameResources[_currentFrameResourceIndex].get();

    if (_currFrameResource->Fence != 0 && _fence->GetCompletedValue() < _currFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(_fence->SetEventOnCompletion(_currFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
    AnimateMaterials(timer);
    UpdateObjectCBs(timer);
    UpdateMaterialsCBs(timer);
    UpdateMainPassCB(timer);
}

void Crate::Draw(const GameTimer& timer)
{
    auto cmdListAlloc = _currFrameResource->CmdListAlloc;
    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(_commandList->Reset(cmdListAlloc.Get(), _opaquePSO.Get()));

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
    _commandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(_commandList.Get(), _opaqueRenderItems);

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    ThrowIfFailed(_commandList->Close());

    ID3D12CommandList* cmdsLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    ThrowIfFailed(_swapChain->Present(0, 0));
    _currBackBuffer = (_currBackBuffer + 1) % _swapChainBufferCount;
    _currFrameResource->Fence = ++_currentFence;

    _commandQueue->Signal(_fence.Get(), _currentFence);
}

void Crate::OnMouseDown(WPARAM btnState, int x, int y)
{
    _lastMousePos.x = x;
    _lastMousePos.y = y;
    SetCapture(_hMainWindow);
}

void Crate::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void Crate::OnMouseMove(WPARAM btnState, int x, int y)
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
        float dx = 0.05f * static_cast<float>(x - _lastMousePos.x);
        float dy = 0.05f * static_cast<float>(y - _lastMousePos.y);

        _radius += dx - dy;

        _radius = MathHelper::Clamp(_radius, 5.0f, 150.0f);
    }
    _lastMousePos.x = x;
    _lastMousePos.y = y;
}

void Crate::OnKeyboardInput(const GameTimer& timer)
{

}

void Crate::UpdateCamera(const GameTimer& timer)
{
    _eyePos.x = _radius * sinf(_phi) * cosf(_theta);
    _eyePos.z = _radius * sinf(_phi) * sinf(_theta);
    _eyePos.y = _radius * cosf(_phi);

    XMVECTOR pos = XMVectorSet(_eyePos.x, _eyePos.y, _eyePos.z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&_view, view);
}

void Crate::AnimateMaterials(const GameTimer& timer)
{
    _rotationAngle += timer.DeltaTime();
    if (_rotationAngle >= MathHelper::Pi * 2)
        _rotationAngle -= MathHelper::Pi * 2;

    XMFLOAT4X4 matTransform = _materials["woodCrate"]->MatTransform;
    XMMATRIX mat = XMLoadFloat4x4(&matTransform);
    mat = XMMatrixTranslation(-0.5f, -0.5f, 0.0f) * XMMatrixRotationZ(_rotationAngle) * XMMatrixTranslation(0.5f, 0.5f, 0.0f);
    XMStoreFloat4x4(&_materials["woodCrate"]->MatTransform, mat);

    _materials["woodCrate"]->NumFramesDirty = FrameResource::NumFrameResources;
}

void Crate::UpdateObjectCBs(const GameTimer& timer)
{
    auto currObjCB = _currFrameResource->ObjectCB.get();
    for (auto& e : _allRenderItems)
    {
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX model = XMLoadFloat4x4(&e->Model);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

            CrateFrameResource::ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.Model, XMMatrixTranspose(model));
            XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

            currObjCB->CopyData(e->ObjCBIndex, objConstants);
            e->NumFramesDirty--;
        }
    }
}

void Crate::UpdateMaterialsCBs(const GameTimer& timer)
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

void Crate::UpdateMainPassCB(const GameTimer& timer)
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

    _mainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
    _mainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
    _mainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
    _mainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
    _mainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
    _mainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

    auto currPassCB = _currFrameResource->PassCB.get();
    currPassCB->CopyData(0, _mainPassCB);
}

void Crate::LoadTextures()
{
    auto woodCrateTex = std::make_unique<Texture>();
    woodCrateTex->Name = "woodCrateTex";    
    woodCrateTex->Filename = L"Textures/WoodCrate01_mod.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), woodCrateTex->Filename.c_str(), woodCrateTex->Resource, woodCrateTex->UploadHeap));

    auto wireFenceTex = std::make_unique<Texture>();
    wireFenceTex->Name = "wireFence";
    wireFenceTex->Filename = L"Textures/WireFence.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), wireFenceTex->Filename.c_str(), wireFenceTex->Resource, wireFenceTex->UploadHeap));

    _textures[woodCrateTex->Name] = move(woodCrateTex);
    _textures[wireFenceTex->Name] = move(wireFenceTex);
}

void Crate::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];
    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

    auto staticSamplers = FrameResource::GetStaticSamplers();

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(4, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(_device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(_rootSignature.GetAddressOf())));
}

void Crate::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 2;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_srvHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(_srvHeap->GetCPUDescriptorHandleForHeapStart());

    auto woodCrateTex = _textures["woodCrateTex"]->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = woodCrateTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = woodCrateTex->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    _device->CreateShaderResourceView(woodCrateTex.Get(), &srvDesc, hDescriptor);

    hDescriptor.Offset(1, _cbvSrvUavDescriptorSize);
    auto fenceTex = _textures["wireFence"]->Resource;
    srvDesc.Format = fenceTex.Get()->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = fenceTex.Get()->GetDesc().MipLevels;

    _device->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);
}

void Crate::BuildShaderAndInputLayout()
{
    _shaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\TexCrate.hlsl", nullptr, "vert", "vs_5_0");
    _shaders["opaquePS"] = D3DUtil::CompileShader(L"Shaders\\TexCrate.hlsl", nullptr, "frag", "ps_5_0");

    _inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void Crate::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = 0;
    boxSubmesh.BaseVertexLocation = 0;

    std::vector<CrateFrameResource::Vertex> vertices(box.Vertices.size());

    for (size_t i = 0; i < box.Vertices.size(); i++)
    {
        vertices[i].Pos = box.Vertices[i].Position;
        vertices[i].Normal = box.Vertices[i].Normal;
        vertices[i].TexC = box.Vertices[i].TexCoord;
    }
    std::vector<uint16_t> indices = box.GetIndices16();
    const UINT vbByteSize = sizeof(CrateFrameResource::Vertex) * (UINT)vertices.size();
    const UINT ibByteSize = sizeof(uint16_t) * (UINT)indices.size();

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "boxGeo";

    geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(CrateFrameResource::Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["box"] = boxSubmesh;
    _geometries[geo->Name] = move(geo);
}

void Crate::BuildPSOs()
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
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&_opaquePSO)));
}

void Crate::BuildFrameResources()
{
    for (int i = 0; i < CrateFrameResource::NumFrameResources; i++)
    {
        _frameResources.push_back(std::make_unique<CrateFrameResource>(_device.Get(), 1, (UINT)_allRenderItems.size(), (UINT)_materials.size()));
    }
}

void Crate::BuildMaterials()
{
    auto woodCrate = std::make_unique<Material>();
    woodCrate->Name = "woodCrate";
    woodCrate->MatCBIndex = 0;
    woodCrate->DiffuseSrvHeapIndex = 0;
    woodCrate->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    woodCrate->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    woodCrate->Roughness = 0.2f;

    _materials["woodCrate"] = move(woodCrate);
}

void Crate::BuildRenderItems()
{
    auto boxRenderItem = std::make_unique<CrateRenderItem>();
    boxRenderItem->ObjCBIndex = 0;
    boxRenderItem->Mat = _materials["woodCrate"].get();
    boxRenderItem->Geo = _geometries["boxGeo"].get();
    boxRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRenderItem->IndexCount = boxRenderItem->Geo->DrawArgs["box"].IndexCount;
    boxRenderItem->StartIndexLocation = boxRenderItem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRenderItem->BaseVertexLocation = boxRenderItem->Geo->DrawArgs["box"].BaseVertexLocation;

    _allRenderItems.push_back(move(boxRenderItem));
    for (auto& e : _allRenderItems)
        _opaqueRenderItems.push_back(e.get());
}

void Crate::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<CrateRenderItem*>& renderItems)
{
    UINT objCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(CrateFrameResource::ObjectConstants));
    UINT matCBByteSize = D3DUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

    auto objectCB = _currFrameResource->ObjectCB->Resource();
    auto matCB = _currFrameResource->MaterialCB->Resource();

    for (size_t i = 0; i < renderItems.size(); i++)
    {
        auto ri = renderItems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        CD3DX12_GPU_DESCRIPTOR_HANDLE tex(_srvHeap->GetGPUDescriptorHandleForHeapStart());
        tex.Offset(ri->Mat->DiffuseSrvHeapIndex, _cbvSrvUavDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

        cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

