//
// Scene with waves and lighting.
//

#pragma once

#include "../../../Core/Application.h"
#include "LitWavesFrameResource.h"
#include "../Waves/Waves.h"
#include "../LitColumns/LitColumnsRenderItem.h"

namespace DX12Samples
{
class LitWaves : public Application
{
public:
    LitWaves(HINSTANCE hInstance);
    LitWaves(const LitWaves& rhs) = delete;
    LitWaves& operator=(const LitWaves& rhs) = delete;
    ~LitWaves() override;
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
     * \brief Move waves and update them vertex buffer.
     */
    void UpdateWaves(const GameTimer& timer);
    /**
     * \brief Build scene main root signature.
     */
    void BuildRootSignature();
    /**
     * \brief Load shaders from hlsl files and construct input layouts.
     */
    void BuildShaderAndInputLayout();
    /**
     * \brief Build mesh for land.
     */
    void BuildLandGeometry();
    /**
     * \brief Build buffers for water.
     */
    void BuildWavesGeometryBuffers();
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
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<LitColumnsRenderItem*>& renderItems);
    /**
     * \brief Height of hills at point x, z.
     */
    float GetHillsHeight(float x, float z) const;
    /**
     * \brief Normalsof hills at point x, z.
     */
    DirectX::XMFLOAT3 GetHillsNormal(float x, float z) const;

private:
    std::vector<std::unique_ptr<LitWavesFrameResource>> _frameResources;
    LitWavesFrameResource* _currFrameResource = nullptr;
    int _currentFrameResourceIndex = 0;

    UINT _cbvSrvDescriptorSize = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> _materials;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> _shaders;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> _PSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;
    LitColumnsRenderItem* _wavesRenderItem = nullptr;

    std::vector<std::unique_ptr<LitColumnsRenderItem>> _allRenderItems;
    std::vector<LitColumnsRenderItem*> _renderItemLayer[(int)LitColumnsRenderItem::RenderLayer::Count];
    std::unique_ptr<Waves> _waves;

    LitWavesFrameResource::PassConstants _mainPassCB;

    bool _isWireframe = false;

    DirectX::XMFLOAT3 _eyePos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4X4 _view = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _proj = MathHelper::Identity4x4();

    float _theta = 1.5f * DirectX::XM_PI;
    float _phi = 0.2f * DirectX::XM_PI;
    float _radius = 15.0f;

    float _sunTheta = 1.25f * DirectX::XM_PI;
    float _sunPhi = DirectX::XM_PIDIV4;

    POINT _lastMousePos;
};
}
