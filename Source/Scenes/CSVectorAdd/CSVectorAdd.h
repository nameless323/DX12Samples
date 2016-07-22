#pragma once

#include "../../../Core/Application.h"
#include "../../../Core/D3DUtil.h"


class CSVectorAdd : public Application
{
public:
    CSVectorAdd(HINSTANCE hInstance);
    CSVectorAdd(const CSVectorAdd& rhs) = delete;
    CSVectorAdd& operator=(const CSVectorAdd& rhs) = delete;
    ~CSVectorAdd() override;

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

    const int NumDataElements = 32;

    Microsoft::WRL::ComPtr<ID3D12Resource> _inputBufferA = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _inputUploadBufferA = nullptr;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> _inputBufferB = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _inputUploadBufferB = nullptr;
    
    Microsoft::WRL::ComPtr<ID3D12Resource> _outputBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _readBackBuffer = nullptr;

    struct Data
    {
        DirectX::XMFLOAT3 v1;
        DirectX::XMFLOAT2 v2;
    };
};
