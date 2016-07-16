#include "IcosahedronGeoTesselation.h"
#include "../../../Core/GeometryGenerator.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;

IcosahedronGeoTesselation::IcosahedronGeoTesselation(HINSTANCE hInstance) : Application(hInstance)
{
}

bool IcosahedronGeoTesselation::Init()
{
    if (!Application::Init())
        return false;

    ThrowIfFailed(_commandAllocator->Reset());
    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    LoadTextures();
    BuildShaderAndInputLayout();
    BuildDescriptorHeaps();
    BuildRootSignature();
    BuildShapeGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    FlushCommandQueue();

    return true;
}

IcosahedronGeoTesselation::~IcosahedronGeoTesselation()
{
    if (_device != nullptr)
        FlushCommandQueue();
}

LRESULT IcosahedronGeoTesselation::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::MsgProc(hwnd, msg, wParam, lParam);
}

int IcosahedronGeoTesselation::Run()
{
    return Application::Run();
}

void IcosahedronGeoTesselation::OnResize()
{
    Application::OnResize();
    XMStoreFloat4x4(&_proj, XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f));
}

void IcosahedronGeoTesselation::Update(const GameTimer& timer)
{
    OnKeyboardInput(timer);
    UpdateCamera(timer);

    _currentFrameResourceIndex = (_currentFrameResourceIndex + 1) % CrateFrameResource::NumFrameResources;
    _currFrameResource = _frameResources[_currentFrameResourceIndex].get();

    if (_currFrameResource->Fence != 0 && _currFrameResource->Fence > _fence->GetCompletedValue())
    {
        HANDLE e = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        ThrowIfFailed(_fence->SetEventOnCompletion(_currFrameResource->Fence, e));
        WaitForSingleObject(e, INFINITE);
        CloseHandle(e);
    }

    UpdateMainPassCB(timer);
    UpdateMaterialsCBs(timer);
    UpdateObjectCBs(timer);
}

void IcosahedronGeoTesselation::Draw(const GameTimer& timer)
{
}

void IcosahedronGeoTesselation::OnMouseDown(WPARAM btnState, int x, int y)
{
    _lastMousePos.x = x;
    _lastMousePos.y = y;
    SetCapture(_hMainWindow);
}

void IcosahedronGeoTesselation::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void IcosahedronGeoTesselation::OnMouseMove(WPARAM btnState, int x, int y)
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
        float dx = 0.01f * static_cast<float>(x - _lastMousePos.x);
        float dy = 0.01f * static_cast<float>(y - _lastMousePos.y);

        _radius += dx - dy;

        _radius = MathHelper::Clamp(_radius, 1.0f, 150.0f);
    }
    _lastMousePos.x = x;
    _lastMousePos.y = y;
}

void IcosahedronGeoTesselation::OnKeyboardInput(const GameTimer& timer)
{
    if (GetAsyncKeyState('1') & 0x8000)
        _isWireframe = true;
    else
        _isWireframe = false;

    if (GetAsyncKeyState('2') & 0x8000)
        _isTesselated = false;
    else
        _isTesselated = true;
}

void IcosahedronGeoTesselation::UpdateCamera(const GameTimer& timer)
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

void IcosahedronGeoTesselation::AnimateMaterials(const GameTimer& timer)
{
}

void IcosahedronGeoTesselation::UpdateObjectCBs(const GameTimer& timer)
{
}

void IcosahedronGeoTesselation::UpdateMaterialsCBs(const GameTimer& timer)
{
}

void IcosahedronGeoTesselation::UpdateMainPassCB(const GameTimer& timer)
{
}

void IcosahedronGeoTesselation::LoadTextures()
{
    auto tex = std::make_unique<Texture>();
    tex->Name = "WoodCrate";
    tex->Filename = L"Textures\\WoodCrate0.dds";
    ThrowIfFailed(CreateDDSTextureFromFile12(_device.Get(), _commandList.Get(), tex->Filename.c_str(), tex->Resource, tex->UploadHeap));
    _textures[tex->Name] = std::move(tex);
}

void IcosahedronGeoTesselation::BuildRootSignature()
{    
    D3D12_DESCRIPTOR_RANGE table;
    table.NumDescriptors = 1;
    table.BaseShaderRegister = 0;
    table.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    table.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    table.RegisterSpace = 0;


    CD3DX12_ROOT_PARAMETER rootParams[3];
    rootParams[0].InitAsDescriptorTable(1, &table, D3D12_SHADER_VISIBILITY_PIXEL);
    rootParams[1].InitAsConstantBufferView(0);
    rootParams[2].InitAsConstantBufferView(1);
    rootParams[3].InitAsConstantBufferView(2);
    auto staticSamplers = CrateFrameResource::GetStaticSamplers();
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = CD3DX12_ROOT_SIGNATURE_DESC(4, rootParams, 6, staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> errorBlob = nullptr;
    ComPtr<ID3DBlob> sigBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, sigBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
        printf((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);
    _device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(_rootSignature.GetAddressOf()));
}

void IcosahedronGeoTesselation::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvHeapDesc.NodeMask = 0;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 1;
    ThrowIfFailed(_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(_srvHeap.GetAddressOf())));
    
    auto texResource = _textures["WoodCrate"].get()->Resource;
    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
    ZeroMemory(&viewDesc, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));
    viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    viewDesc.Format = texResource->GetDesc().Format;
    viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MipLevels = texResource->GetDesc().MipLevels;
    viewDesc.Texture2D.MostDetailedMip = 0;
    viewDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    _device->CreateShaderResourceView(texResource.Get(), &viewDesc, _srvHeap->GetCPUDescriptorHandleForHeapStart());
}

void IcosahedronGeoTesselation::BuildShaderAndInputLayout()
{
    _shaders["standardVS"] = D3DUtil::CompileShader(L"Shaders\\IcosahedronSimple.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["standardPS"] = D3DUtil::CompileShader(L"Shaders\\IcosahedronSimple.hlsl", nullptr, "frag", "ps_5_1");

    _shaders["tessVS"] = D3DUtil::CompileShader(L"Shaders\\IcosahedronGeomTess.hlsl", nullptr, "vert", "vs_5_1");
    _shaders["tessGS"] = D3DUtil::CompileShader(L"Shaders\\IcosahedronGeomTess.hlsl", nullptr, "geom", "gs_5_1");
    _shaders["tessPS"] = D3DUtil::CompileShader(L"Shaders\\IcosahedronGeomTess.hlsl", nullptr, "frag", "ps_5_1");

    D3D12_INPUT_ELEMENT_DESC pos;
    pos.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    pos.AlignedByteOffset = 0;
    pos.InputSlot = 0;
    pos.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    pos.InstanceDataStepRate = 0;
    pos.SemanticIndex = 0;
    pos.SemanticName = "POSITION";

    D3D12_INPUT_ELEMENT_DESC norm = pos;
    norm.AlignedByteOffset = 12;
    norm.SemanticName = "NORMAL";

    D3D12_INPUT_ELEMENT_DESC uv = pos;
    uv.AlignedByteOffset = 24;
    uv.Format = DXGI_FORMAT_R32G32_FLOAT;
    uv.SemanticName = "TEXCOORD";

    _inputLayout = { pos, norm, uv };
}

void IcosahedronGeoTesselation::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
    auto icoGeo = geoGen.CreateGeosphere(1.0f, 4);
    std::vector<CrateFrameResource::Vertex> verts;
    for (int i = 0; i < icoGeo.Vertices.size(); i++)
    {
        CrateFrameResource::Vertex v;
        v.Pos = icoGeo.Vertices[i].Position;
        v.Normal = icoGeo.Vertices[i].Normal;
        v.TexC = icoGeo.Vertices[i].TexCoord;
        verts.push_back(v);
    }
    auto indices = icoGeo.GetIndices16();

    size_t vertSize = sizeof(GeometryGenerator::Vertex) * verts.size();
    size_t indSize = sizeof(uint16_t) * indices.size();
    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "icosahedron";

    geo->VertexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), verts.data(), vertSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = D3DUtil::CreateDefaultBuffer(_device.Get(), _commandList.Get(), indices.data(), indSize, geo->IndexBufferUploader);

    geo->IndexBufferByteSize = indSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->VertexBufferByteSize = vertSize;
    geo->VertexByteStride = sizeof(GeometryGenerator::Vertex);
    
    SubmeshGeometry submesh;
    submesh.BaseVertexLocation = 0;
    submesh.StartIndexLocation = 0;
    submesh.IndexCount = indices.size();

    geo->DrawArgs["ico"] = submesh;
    
    _geometries[geo->Name] = move(geo);
}

void IcosahedronGeoTesselation::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePso;
    ZeroMemory(&opaquePso, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePso.VS = { reinterpret_cast<BYTE*>(_shaders["standardVS"]->GetBufferPointer()), _shaders["standardVS"]->GetBufferSize() };
    opaquePso.PS = { reinterpret_cast<BYTE*>(_shaders["standardPS"]->GetBufferPointer()), _shaders["standardPS"]->GetBufferSize() };
    opaquePso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePso.DSVFormat = _dsvFormat;
    opaquePso.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    opaquePso.pRootSignature = _rootSignature.Get();
    
    opaquePso.InputLayout = { _inputLayout.data(), _inputLayout.size() };
    opaquePso.NodeMask = 0;
    opaquePso.SampleMask = UINT_MAX;
    opaquePso.SampleDesc.Count = _4xMsaa ? 4 : 1;
    opaquePso.SampleDesc.Quality = _4xMsaa ? _4xMsaaQuality - 1 : 0;
    opaquePso.NumRenderTargets = 1;
    opaquePso.RTVFormats[0] = _backBufferFormat;
    opaquePso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&opaquePso, IID_PPV_ARGS(_PSOs["standard"].GetAddressOf())));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC wireframePso = opaquePso;
    wireframePso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    wireframePso.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&wireframePso, IID_PPV_ARGS(_PSOs["wireframe"].GetAddressOf())));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC geomTessPso = opaquePso;
    geomTessPso.VS = { reinterpret_cast<BYTE*>(_shaders["tessVS"]->GetBufferPointer()), _shaders["tessVS"]->GetBufferSize() };
    geomTessPso.GS = { reinterpret_cast<BYTE*>(_shaders["tessGS"]->GetBufferPointer()), _shaders["tessGS"]->GetBufferSize() };
    geomTessPso.PS = { reinterpret_cast<BYTE*>(_shaders["tessPS"]->GetBufferPointer()), _shaders["tessPS"]->GetBufferSize() };
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&geomTessPso, IID_PPV_ARGS(_PSOs["geomTess"].GetAddressOf())));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC wireframeGeomTessPso = geomTessPso;
    wireframeGeomTessPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    wireframeGeomTessPso.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(_device->CreateGraphicsPipelineState(&wireframeGeomTessPso, IID_PPV_ARGS(_PSOs["geomTessWireframe"].GetAddressOf())));
}

void IcosahedronGeoTesselation::BuildFrameResources()
{
    for (int i = 0; i < CrateFrameResource::NumFrameResources; i++)
        _frameResources.push_back(std::make_unique<CrateFrameResource>(_device.Get(), 1, _allRenderItems.size(), _materials.size()));
}

void IcosahedronGeoTesselation::BuildMaterials()
{
    auto m = std::make_unique<Material>();
    m->Name = "WoodCrate";
    m->DiffuseAlbedo = XMFLOAT4(1.0, 1.0, 1.0, 1.0);
    m->DiffuseSrvHeapIndex = 0;
    m->MatCBIndex = 0;
    m->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    m->Roughness = 0.5f;
    m->MatTransform = MathHelper::Identity4x4();

    m->NumFramesDirty = CrateFrameResource::NumFrameResources;
    _materials[m->Name] = move(m);
}

void IcosahedronGeoTesselation::BuildRenderItems()
{
    auto renderItem = std::make_unique<CrateRenderItem>();
    renderItem->Geo = _geometries["icosahedron"].get();
    renderItem->Mat = _materials["WoodCrate"].get();
    renderItem->Model = MathHelper::Identity4x4();
    renderItem->ObjCBIndex = 0;
    renderItem->TexTransform = MathHelper::Identity4x4();
    renderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    renderItem->BaseVertexLocation = renderItem->Geo->DrawArgs["ico"].BaseVertexLocation;
    renderItem->StartIndexLocation = renderItem->Geo->DrawArgs["ico"].StartIndexLocation;
    renderItem->IndexCount = renderItem->Geo->DrawArgs["ico"].IndexCount;

    _opaqueRenderItems.push_back(renderItem.get());
    _allRenderItems.push_back(move(renderItem));
}

void IcosahedronGeoTesselation::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<CrateRenderItem*>& renderItems)
{
}
