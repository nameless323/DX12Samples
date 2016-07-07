#pragma once

#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"

#include "../../Common/RenderItem.h"
#include "StencilingFrameResource.h"

class Stenciling : public Application
{
public:
    Stenciling(HINSTANCE hInstance);
    Stenciling(const Stenciling& rhs) = delete;
    Stenciling& operator=(const Stenciling& rhs) = delete;
    ~Stenciling() override;

    bool Init() override;
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    int Run() override;
protected:
    void OnResize() override;
    void Update(const GameTimer& timer) override;
    void Draw(const GameTimer& timer) override;

    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;
    void OnMouseMove(WPARAM btnState, int x, int y) override;

    void OnKeyboardInput(const GameTimer& timer);
    void UpdateCamera(const GameTimer& timer);
    void AnimateMaterials(const GameTimer& timer);
    void UpdateObjectCBs(const GameTimer& timer);
    void UpdateMaterialCBs(const GameTimer& timer);
    void UpdateMainPassCB(const GameTimer& timer);
    void UpdateReflectedPassCB(const GameTimer& timer);

    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShaderAndInputLayout();
    void BuildRoomGeometry();
    void BuildSkullGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems);

private:
    std::vector<std::unique_ptr<StencilingFrameResource>> _frameResources;
    StencilingFrameResource* _currFrameResource = nullptr;
    int _currentFrameResourceIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _srvHeap = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> _materials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> _shaders;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> _PSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;

    RenderItem* _skullRenderItem = nullptr;
    RenderItem* _reflectedSkullRenderItem = nullptr;
    RenderItem* _shadowedSkullRenderItem = nullptr;

    std::vector<std::unique_ptr<RenderItem>> _allRenderItems;
    std::vector<RenderItem*> _renderItemLayer[(int)RenderItem::RenderLayer::Count];

    StencilingFrameResource::PassConstants _mainPassCB;
    StencilingFrameResource::PassConstants _reflectedPassCB;
    DirectX::XMFLOAT3 _skullTranslation = { 0.0f, 1.0f, -5.0f };

    bool _isWireframe = false;

    DirectX::XMFLOAT3 _eyePos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4X4 _view = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _proj = MathHelper::Identity4x4();

    float _theta = 1.24f * DirectX::XM_PI;
    float _phi = 0.42f * DirectX::XM_PI;
    float _radius = 12.0f;

    float _sunTheta = 1.25f * DirectX::XM_PI;
    float _sunPhi = DirectX::XM_PIDIV4;

    POINT _lastMousePos;
};
