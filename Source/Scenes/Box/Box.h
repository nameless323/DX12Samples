//
// Simple scene with simple box.
//

#pragma once

#include <windows.h>

#include "../../../Core/UploadBuffer.h"
#include "../../../Core/Application.h"

namespace DX12Samples
{
/**
 * \brief Box vertex data.
 */
struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT4 Color;
};
/**
 * \brief Unique object data.
 */
struct ObjectConstants
{
    DirectX::XMFLOAT4X4 MVP = MathHelper::Identity4x4();
};

class Box : public Application
{
public:    
    Box(HINSTANCE hInstance);
    /**
     * \brief Scene initiallization including texture loading, loading all pipline etc.
     */
    bool Init() override;
    ~Box() override;
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
private:
    /**
     * \brief Build nesessary deccriptor heaps for scene.
     */
    void BuildDescriptorHeaps();
    /**
     * \brief Build level constant buffers.
     */
    void BuildConstantBuffers();
    /**
     * \brief Build scene main root signature.
     */
    void BuildRootSignature();
    /**
     * \brief Load shaders from hlsl files and construct input layouts.
     */
    void BuildShadersAndInputLayout();
    /**
     * \brief Build mesh for box.
     */
    void BuildBoxGeometry();
    /**
     * \brief Build pipline state objects.
     */
    void BuildPSO();

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _cbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>> _objectCB = nullptr;
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
}