//
// Using compute shaders to add bunch of vectors.
//

#pragma once

#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"

namespace DX12Samples
{
class CsVecLen : public Application
{
public:
    CsVecLen(HINSTANCE hInstance);
    CsVecLen(const CsVecLen& rhs) = delete;
    CsVecLen& operator=(const CsVecLen& rhs) = delete;
    ~CsVecLen() override;
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
     * \brief Execute computations.
     */
    void DoComputeWork();
    /**
     * \brief Build buffers for computaton data.
     */
    void BuildBuffers();
    /**
     * \brief Build scene main root signature.
     */
    void BuildRootSignature();
    /**
     * \brief Build scene main root signature.
     */
    void BuildShaderAndInputLayout();
    /**
     * \brief Build pipline state objects.
     */
    void BuildPSOs();

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature = nullptr;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> _shaders;
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> _PSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> _inputLayout;

    static const int NumDataElements = 64;

    Microsoft::WRL::ComPtr<ID3D12Resource> _inputBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _inputUploadBuffer = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> _outputBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _readBackBuffer = nullptr;
};
}
