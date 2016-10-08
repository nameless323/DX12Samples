//
// Creates cylinder with geometry shader.
//

#pragma once

#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"
#include "../Crate/CrateRenderItem.h"
#include "../Crate/CrateFrameResource.h"

namespace DX12Samples
{
class GeomCylinder : public Application
{
public:
    GeomCylinder(HINSTANCE hInstance);
    GeomCylinder(const GeomCylinder& rhs) = delete;
    GeomCylinder& operator=(const GeomCylinder& rhs) = delete;
    /**
     * \brief Scene initiallization including texture loading, loading all pipline etc.
     */
    bool Init() override;
    ~GeomCylinder() override;
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
     * \brief Update pass buffer.
     */
    void UpdateMainPassCB(const GameTimer& timer);
    /**
     * \brief Update objects constant buffers for current frame.
     */
    void UpdateObjectCBs(const GameTimer& timer);
    /**
     * \brief Build scene main root signature.
     */
    void BuildRootSignature();
    /**
     * \brief Load shaders from hlsl files and construct input layouts.
     */
    void BuildShaderAndInputLayout();
    /**
     * \brief Build level geometry.
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
     * \brief Build scene objects.
     */
    void BuildRenderItems();
    /**
     * \brief Draw scene objects.
     */
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<CrateRenderItem*>& renderItems);

private:
    std::vector<std::unique_ptr<CrateFrameResource>> _frameResources;
    CrateFrameResource* _currFrameResource = nullptr;
    int _currentFrameResourceIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _srvHeap = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> _shaders;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> _opaquePSO = nullptr;

    std::vector<std::unique_ptr<CrateRenderItem>> _allRenderItems;
    std::vector<CrateRenderItem*> _opaqueRenderItems;

    CrateFrameResource::PassConstants _mainPassCB;

    DirectX::XMFLOAT3 _eyePos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4X4 _view = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _proj = MathHelper::Identity4x4();

    float _theta = 1.3f * DirectX::XM_PI;
    float _phi = 0.4f * DirectX::XM_PI;
    float _radius = 2.5f;

    float _rotationAngle = 0.0f;

    POINT _lastMousePos;
};
}