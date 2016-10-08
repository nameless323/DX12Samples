#pragma once

#include "ShapesRenderItem.h"
#include "../../../Core/Application.h"
#include "ShapesFrameResource.h"
#include "../../../Core/GeometryGenerator.h"

namespace DX12Samples
{
class Shapes : public Application
{
public:
    Shapes(HINSTANCE hInstance);
    bool Init() override;
    ~Shapes() override;
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
    void UpdateMainPassCB(const GameTimer& timer);

    void BuildDescriptorHeaps();
    void BuildConstantBufferViews();
    void BuildRootSignature();
    void BuildShaderAndInputLayout();
    void BuildShapeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<ShapesRenderItem*>& renderItems);
    GeometryGenerator::MeshData ParseFile(std::string filename) const;

private:
    std::vector<std::unique_ptr<ShapesFrameResource>> _frameResources;
    ShapesFrameResource* _currFrameResource = nullptr;
    int _currentFrameResourceIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _cbvHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _srvHeap = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> _geometries;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> _shaders;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> _PSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;
    std::vector<std::unique_ptr<ShapesRenderItem>> _allRenderItems;
    std::vector<ShapesRenderItem*> _opaqueRenderItems;

    PassConstants _mainPassCB;
    UINT _passCbvOffset = 0;
    bool _isWireframe = false;

    DirectX::XMFLOAT3 _eyePos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4X4 _view = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _proj = MathHelper::Identity4x4();

    float _theta = 1.5f * DirectX::XM_PI;
    float _phi = 0.2f * DirectX::XM_PI;
    float _radius = 15.0f;

    POINT _lastMousePos;
};
}