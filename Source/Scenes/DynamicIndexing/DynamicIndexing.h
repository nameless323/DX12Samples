//
// Using dynamic indexing in texture array on GPU.
//

#pragma once

#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"

#include "../../Common/RenderItem.h"
#include "DynamicIndexingFrameResource.h"
#include "../../../Core/Camera.h"

namespace DX12Samples
{
class DynamicIndexing : public Application
{
public:
    DynamicIndexing(HINSTANCE hInstance);
    DynamicIndexing(const DynamicIndexing& rhs) = delete;
    DynamicIndexing& operator=(const DynamicIndexing& rhs) = delete;
    /**
     * \brief Scene initiallization including texture loading, loading all pipline etc.
     */
    bool Init() override;
    ~DynamicIndexing() override;
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
     * \brief Load shaders from hlsl files and construct input layouts.
     */
    void BuildShaderAndInputLayout();
    /**
     * \brief Build mesh for scene objects.
     */
    void BuildShapeGeometry();
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
}