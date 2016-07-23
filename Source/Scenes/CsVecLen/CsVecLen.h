#pragma once

#pragma once

#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"


class CsVecLen : public Application
{
public:
    CsVecLen(HINSTANCE hInstance);
    CsVecLen(const CsVecLen& rhs) = delete;
    CsVecLen& operator=(const CsVecLen& rhs) = delete;
    ~CsVecLen() override;

    bool Init() override;
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    int Run() override;
protected:
    void OnResize() override;
    void Update(const GameTimer& timer) override;
    void Draw(const GameTimer& timer) override;

    void DoComputeWork();
    void BuildBuffers();
    void BuildRootSignature();
    void BuildShaderAndInputLayout();
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