#pragma once
#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"

#include "../../Common/RenderItem.h"
#include "ShadowmappingFrameResource.h"
#include "../../../Core/Camera.h"
#include "ShadowMap.h"

class Shadowmapping : public Application
{
public:
    Shadowmapping(HINSTANCE hInstance);
    Shadowmapping(const Shadowmapping& rhs) = delete;
    Shadowmapping& operator=(const Shadowmapping& rhs) = delete;
    bool Init() override;
    ~Shadowmapping() override;
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    int Run() override;
protected:
    void CreateRtvAndDsvDescriptorHeaps() override;
    void OnResize() override;
    void Update(const GameTimer& timer) override;
    void Draw(const GameTimer& timer) override;

    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;
    void OnMouseMove(WPARAM btnState, int x, int y) override;

    void OnKeyboardInput(const GameTimer& timer);
    void AnimateMaterials(const GameTimer& timer);
    void UpdateObjectCBs(const GameTimer& timer);
    void UpdateMaterialBuffer(const GameTimer& timer);
    void UpdateShadowTransform(const GameTimer& timer);
    void UpdateMainPassCB(const GameTimer& timer);
    void UpdateShadowPassCB(const GameTimer& timer);

    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShaderAndInputLayout();
    void BuildShapeGeometry();
    void BuildSkullGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems);
    void DrawSceneToShadowMap();

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
private:
    std::vector<std::unique_ptr<ShadowmappingFrameResource>> _frameResources;
    ShadowmappingFrameResource* _currFrameResource = nullptr;
    int _currentFrameResourceIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _srvHeap = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> _materials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> _shaders;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> _PSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;


    std::vector<std::unique_ptr<RenderItem>> _allRenderItems;
    std::vector<RenderItem*> _renderItemLayer[(int)RenderItem::RenderLayer::Count];

    UINT _skyTexHeapIndex = 0;
    UINT _shadowMapHeapIndex = 0;

    UINT _nullCubeSrvIndex = 0;
    UINT _nullTexSrvIndex = 0;

    CD3DX12_GPU_DESCRIPTOR_HANDLE _nullSrv;

    ShadowmappingFrameResource::PassConstants _mainPassCB;
    ShadowmappingFrameResource::PassConstants _shadowPassCB;

    Camera _camera;
    std::unique_ptr<ShadowMap> _shadowMap;
    DirectX::BoundingSphere _sceneBounds;

    float _lightNearZ = 0.0f;
    float _lightFarZ = 0.0f;
    DirectX::XMFLOAT3 _lightPosW;
    DirectX::XMFLOAT4X4 _lightView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _lightProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _shadowTransform = MathHelper::Identity4x4();

    float _lightRotationAngle = 0.0f;
    DirectX::XMFLOAT3 _baseLightDirections[3] =
    {
        DirectX::XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
        DirectX::XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
        DirectX::XMFLOAT3(0.0f, -0.707f, -0.707f)
    };
    DirectX::XMFLOAT3 _rotatedLightDirections[3];

    POINT _lastMousePos;
};

