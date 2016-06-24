#pragma once
#include "../../../Core/Application.h"
#include "LitWavesFrameResource.h"
#include "LitWaves.h"
#include "../Waves/Waves.h"
#include "../LitColumns/LitColumnsRenderItem.h"

class LitWaves : public Application
{
public:
    LitWaves(HINSTANCE hInstance);
    LitWaves(const LitWaves& rhs) = delete;
    LitWaves& operator=(const LitWaves& rhs) = delete;
    ~LitWaves() override;

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
    void UpdateObjectCBs(const GameTimer& timer);
    void UpdateMaterialCBs(const GameTimer& timer);
    void UpdateMainPassCB(const GameTimer& timer);
    void UpdateWaves(const GameTimer& timer);

    void BuildRootSignature();
    void BuildShaderAndInputLayout();
    void BuildLandGeometry();
    void BuildWavesGeometryBuffers();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<LitColumnsRenderItem*>& renderItems);

    float GetHillsHeight(float x, float z) const;
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

