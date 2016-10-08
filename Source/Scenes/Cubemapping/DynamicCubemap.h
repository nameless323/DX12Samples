#pragma once

#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"
#include "../../Common/RenderItem.h"
#include "CubemappingFrameResource.h"
#include "../../../Core/Camera.h"
#include "CubeRenderTarget.h"

namespace DX12Samples
{
class DynamicCubemap : public Application
{
public:
    DynamicCubemap(HINSTANCE hInstance);
    DynamicCubemap(const DynamicCubemap& rhs) = delete;
    DynamicCubemap& operator=(const DynamicCubemap& rhs) = delete;
    bool Init() override;
    ~DynamicCubemap() override;
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    int Run() override;

protected:
    void OnResize() override;
    void Update(const GameTimer& timer) override;
    void Draw(const GameTimer& timer) override;

    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;
    void OnMouseMove(WPARAM btnState, int x, int y) override;
    void CreateRtvAndDsvDescriptorHeaps() override;

    void OnKeyboardInput(const GameTimer& timer);
    void AnimateMaterials(const GameTimer& timer);
    void UpdateObjectCBs(const GameTimer& timer);
    void UpdateMaterialBuffer(const GameTimer& timer);
    void UpdateMainPassCB(const GameTimer& timer);
    void UpdateCubemapFaceCBs();

    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildCubeDepthStencil();
    void BuildShaderAndInputLayout();
    void BuildShapeGeometry();
    void BuildSkullGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems);
    void DrawSceneToCubemap();

    void BuildCubeFaceCamera(float x, float y, float z);

private:
    std::vector<std::unique_ptr<CubemappingFrameResource>> _frameResources;
    CubemappingFrameResource* _currFrameResource = nullptr;
    int _currentFrameResourceIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _srvHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _cubeDepthStencil;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> _materials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> _shaders;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> _PSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;

    std::vector<std::unique_ptr<RenderItem>> _allRenderItems;
    std::vector<RenderItem*> _renderItemLayer[(int)RenderItem::RenderLayer::Count];

    UINT _skyTexHeapIndex = 0;
    UINT _dynamicTexHeapIndex = 0;

    RenderItem* _skullRenderItem = nullptr;
    std::unique_ptr<CubeRenderTarget> _dynamicCubemap = nullptr;
    CD3DX12_CPU_DESCRIPTOR_HANDLE _cubeDsv;

    CubemappingFrameResource::PassConstants _mainPassCB;

    Camera _camera;
    Camera _cubeMapCamera[6];

    POINT _lastMousePos;

    const UINT CubeMapSize = 512;
};
}