#pragma once

#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"
#include "../Crate/CrateRenderItem.h"

namespace DX12Samples
{
class IcosahedronGeoTesselation : public Application
{
public:
    IcosahedronGeoTesselation(HINSTANCE hInstance);
    IcosahedronGeoTesselation(const IcosahedronGeoTesselation& rhs) = delete;
    IcosahedronGeoTesselation& operator=(const IcosahedronGeoTesselation& rhs) = delete;
    bool Init() override;
    ~IcosahedronGeoTesselation() override;
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
    void AnimateMaterials(const GameTimer& timer);
    void UpdateObjectCBs(const GameTimer& timer);
    void UpdateMaterialsCBs(const GameTimer& timer);
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
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<CrateRenderItem*>& renderItems);

private:
    std::vector<std::unique_ptr<CrateFrameResource>> _frameResources;
    CrateFrameResource* _currFrameResource = nullptr;
    int _currentFrameResourceIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _srvHeap = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> _materials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> _shaders;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> _PSOs;


    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;
    
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

    bool _isWireframe = false;
    bool _isGeomTesselated = true;
    bool _isTesselated = false;
    bool _isExplode = false;

    POINT _lastMousePos;
};
}
