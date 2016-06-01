#pragma once
#pragma once
#include <windows.h>
#include "../../Core/Application.h"
#include "../../Core/UploadBuffer.h"



class Ch6Ex : public Application
{
public:
    struct VertexPosData
    {
        DirectX::XMFLOAT3 Pos;
    };

    struct VertexColorData
    {        
        DirectX::XMFLOAT4 Color;
    };

    struct ObjectConstants
    {
        DirectX::XMFLOAT4X4 MVP = MathHelper::Identity4x4();
    };


    Ch6Ex(HINSTANCE hInstance);
    bool Init() override;
    ~Ch6Ex() override;
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    int Run() override;
protected:
    void OnResize() override;
    void Update(const GameTimer& timer) override;
    void Draw(const GameTimer& timer) override;

    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;
    void OnMouseMove(WPARAM btnState, int x, int y) override;
private:
    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry();
    void BuildPSO();

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _cbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>> _objectCB = nullptr;
    std::unique_ptr<UploadBuffer<float>> _timeCB = nullptr;
    std::unique_ptr<MeshGeometry> _boxGeo = nullptr;

    Microsoft::WRL::ComPtr<ID3DBlob> _vsByteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> _psByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> _PSO = nullptr;

    DirectX::XMFLOAT4X4 _model = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _view = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 _proj = MathHelper::Identity4x4();

    float _theta = 1.5f * DirectX::XM_PI;
    float _phi = DirectX::XM_PIDIV4;
    float _radius = 5.0f;

    POINT _lastMousePos;
};
