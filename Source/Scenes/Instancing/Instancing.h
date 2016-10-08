//
// Using instancin for rendering same geometry many times.
//

#pragma once

#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"
#include "InstancingRenderItem.h"
#include "InstancingFrameResource.h"
#include "../../../Core/Camera.h"

namespace DX12Samples
{
class Instancing : public Application
{
public:
    Instancing(HINSTANCE hInstance);
    Instancing(const Instancing& rhs) = delete;
    Instancing& operator=(const Instancing& rhs) = delete;
    /**
     * \brief Scene initiallization including texture loading, loading all pipline etc.
     */
    bool Init() override;
    ~Instancing() override;
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
     * \brief Update data for instance.
     */
    void UpdateInstanceData(const GameTimer& timer);
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
     * \brief Build geometry for sull.
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

    const int _numInstances = 8 * 8 * 8;

    POINT _lastMousePos;
};
}