//
// Scene with bezier patch.
//

#pragma once

#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"
#include "../../Common/RenderItem.h"
#include "../Blending/FrameResourceBlending.h"

namespace DX12Samples
{
class BezierPatch : public Application
{
public:
    BezierPatch(HINSTANCE hInstance);
    BezierPatch(const BezierPatch& rhs) = delete;
    BezierPatch& operator=(const BezierPatch& rhs) = delete;
    ~BezierPatch() override;
    /**
     * \brief Scene initiallization including texture loading, loading all pipline etc.
     */
    bool Init() override;
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
     * \brief Move camera.
     */
    void UpdateCamera(const GameTimer& timer);
    /**
     * \brief Update objects constant buffers for current frame.
     */
    void UpdateObjectCBs(const GameTimer& timer);
    /**
     * \brief Update materials constant buffers for current frame.
     */
    void UpdateMaterialCBs(const GameTimer& timer);
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
     * \brief Buld geometry for bezier patch.
     */
    void BuildQuadPatchGeometry();
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
    std::vector<std::unique_ptr<FrameResourceBlending>> _frameResources;
    FrameResourceBlending* _currFrameResource = nullptr;
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
    FrameResourceBlending::PassConstants _mainPassCB;

    bool _isWireframe = false;

    DirectX::XMFLOAT3 _eyePos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4X4 _view = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _proj = MathHelper::Identity4x4();

    float _theta = 1.5f * DirectX::XM_PI;
    float _phi = DirectX::XM_PIDIV2 - 0.1f;
    float _radius = 50.0f;

    POINT _lastMousePos;
};
}