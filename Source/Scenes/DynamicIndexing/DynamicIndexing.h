#pragma once
#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"

#include "../../Common/RenderItem.h"
#include "DynamicIndexingFrameResource.h"
#include "../../../Core/Camera.h"

class DynamicIndexing : public Application
{
public:
    DynamicIndexing(HINSTANCE hInstance);
    DynamicIndexing(const DynamicIndexing& rhs) = delete;
    DynamicIndexing& operator=(const DynamicIndexing& rhs) = delete;
    bool Init() override;
    ~DynamicIndexing() override;
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
    void UpdateObjectCBs(const GameTimer& timer);
    void UpdateMaterialBuffer(const GameTimer& timer);
    void UpdateMainPassCB(const GameTimer& timer);

    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShaderAndInputLayout();
    void BuildShapeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems);

private:
    std::vector<std::unique_ptr<DynamicIndexingFrameResource>> _frameResources;
    DynamicIndexingFrameResource* _currFrameResource = nullptr;
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
    std::vector<RenderItem*> _opaqueRenderItems;

    DynamicIndexingFrameResource::PassConstants _mainPassCB;

    Camera _camera;

    POINT _lastMousePos;
};
