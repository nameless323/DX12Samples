#pragma once
#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"

#include "InstancingRenderItem.h"
#include "InstancingFrameResource.h"
#include "../../../Core/Camera.h"

class Instancing : public Application
{
public:
    Instancing(HINSTANCE hInstance);
    Instancing(const Instancing& rhs) = delete;
    Instancing& operator=(const Instancing& rhs) = delete;
    bool Init() override;
    ~Instancing() override;
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
    void AnimateMaterials(const GameTimer& timer);
    void UpdateInstanceData(const GameTimer& timer);
    void UpdateMaterialBuffer(const GameTimer& timer);
    void UpdateMainPassCB(const GameTimer& timer);

    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShaderAndInputLayout();
    void BuildSkullGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<InstancingRenderItem*>& renderItems);

private:
    std::vector<std::unique_ptr<InstancingFrameResource>> _frameResources;
    InstancingFrameResource* _currFrameResource = nullptr;
    int _currentFrameResourceIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _srvHeap = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> _materials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> _shaders;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> _PSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;

    InstancingFrameResource::PassConstants _mainPassCB;

    std::vector<std::unique_ptr<InstancingRenderItem>> _allRenderItems;
    std::vector<InstancingRenderItem*> _opaqueRenderItems;

    bool _frustumCullingEnabled = true;
    DirectX::BoundingFrustum _camFrustum;
    InstancingFrameResource::PassConstants _passCB;
    Camera _camera;

    POINT _lastMousePos;
};

