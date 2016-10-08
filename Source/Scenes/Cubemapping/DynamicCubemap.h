//
// Using cube maps for object dynamic reflections.
//

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
    /**
     * \brief Scene initiallization including texture loading, loading all pipline etc.
     */
    bool Init() override;
    ~DynamicCubemap() override;
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    int Run() override;

protected:
    /**
     * \brief Calls when window are resized to rebuild size dependent resources.
     */
    void OnResize() override;
    /**
     * \brief Update game logic.
     */
    void Update(const GameTimer& timer) override;
    /**
     * \brief Draw scene.
     */
    void Draw(const GameTimer& timer) override;
    /**
     * \brief Calls when mouse button down.
     */
    void OnMouseDown(WPARAM btnState, int x, int y) override;
    /**
     * \brief Calls when mouse button up.
     */
    void OnMouseUp(WPARAM btnState, int x, int y) override;
    /**
     * \brief Calls when mouse moves.
     */
    void OnMouseMove(WPARAM btnState, int x, int y) override;
    /**
     * \brief Create RTV and DSV heaps with additional place for cubemap RTVs and DSV.
     */
    void CreateRtvAndDsvDescriptorHeaps() override;
    /**
     * \brief Handle keyboard input.
     */
    void OnKeyboardInput(const GameTimer& timer);
    /**
     * \brief Animate materials e.g water.
     */
    void AnimateMaterials(const GameTimer& timer);
    /**
     * \brief Update objects constant buffers for current frame.
     */
    void UpdateObjectCBs(const GameTimer& timer);
    /**
     * \brief Update materials constant buffers for current frame.
     */
    void UpdateMaterialBuffer(const GameTimer& timer);
    /**
     * \brief Update pass buffer.
     */
    void UpdateMainPassCB(const GameTimer& timer);
    /**
     * \brief Update constant buffers for cubemap faces.
     */
    void UpdateCubemapFaceCBs();
    /**
     * \brief Load scene texutres from dds.
     */
    void LoadTextures();
    /**
     * \brief Build scene main root signature.
     */
    void BuildRootSignature();
    /**
     * \brief Build nesessary deccriptor heaps for scene.
     */
    void BuildDescriptorHeaps();
    /**
     * \brief Build depth stencil resource for cubemap.
     */
    void BuildCubeDepthStencil();
    /**
     * \brief Load shaders from hlsl files and construct input layouts.
     */
    void BuildShaderAndInputLayout();
    /**
     * \brief Build geometry for scene objects.
     */
    void BuildShapeGeometry();
    /**
     * \brief Build geometry for skull.
     */
    void BuildSkullGeometry();
    /**
     * \brief Build pipline state objects.
     */
    void BuildPSOs();
    /**
     * \brief Build nessesary amount for frame resources.
     */
    void BuildFrameResources();
    /**
     * \brief Build materials for scene.
     */
    void BuildMaterials();
    /**
     * \brief Build scene objects.
     */
    void BuildRenderItems();
    /**
     * \brief Draw scene objects.
     */
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& renderItems);
    /**
     * \brief Draw scene to cubemap.
     */
    void DrawSceneToCubemap();

    /**
     * \brief Build cubmap camera at x, y, z position.
     */
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