#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "D3DUtil.h"
#include "GameTimer.h"

class Application
{
public:
    Application(HINSTANCE hInstance);
    Application(const Application& rhs) = delete;
    Application& operator= (const Application& rhs) = delete;
    virtual ~Application();
    static Application* GetApplication();
    HINSTANCE AppInstance() const;
    HWND MainWindow() const;
    float AspectRatio() const;

    bool Get4xMsaaState() const;
    void Set4xMsaaState(bool value);

    virtual int Run();

    virtual bool Init();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    virtual void CreateRtvAndDsvDescriptorHeaps();
    virtual void OnResize();
    virtual void Update(const GameTimer& timer) abstract;
    virtual void Draw(const GameTimer& timer) abstract;

    virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
    virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
    virtual void OnMouseMove(WPARAM btnState, int x, int y) {}

    bool InitMainWindow();
    bool InitDirectX();
    void CreateCommandObjects();
    void CreateSwapChain();

    void FlushCommandQueue();

    ID3D12Resource* CurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    void CalbulateFrameStats() const;

    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

    static Application* _app;
    HINSTANCE _hAppInstance = nullptr;
    HWND _hMainWindow = nullptr;
    bool _isPaused = false;
    bool _isMinimized = false;
    bool _isMaximized = false;
    bool _isResizing = false;
    bool _isInFullscreen = false;

    bool _4xMsaa = false;
    UINT _4xMsaaQuality = 0;

    GameTimer _timer;

    Microsoft::WRL::ComPtr<IDXGIFactory4> _dxgiFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain> _swapChain;
    Microsoft::WRL::ComPtr<ID3D12Device> _device;

    Microsoft::WRL::ComPtr<ID3D12Fence> _fence;
    UINT64 _currentFence = 0;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> _commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> _commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _commandList;

    static const int _swapChainBufferCount = 2;
    int _currBackBuffer = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> _swapChainBuffer[_swapChainBufferCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> _depthStencilBuffer;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _dsvHeap;

    D3D12_VIEWPORT _screenViewport;
    D3D12_RECT _scissorRect;

    UINT _rtvDescriptorSize = 0;
    UINT _dsvDescriptorSize = 0;
    UINT _cbvSrvUavDescriptorSize = 0;

    std::wstring _mainWindowCaption = L"DirectX Application";
    D3D_DRIVER_TYPE _d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT _backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT _dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    int _clientWidth = 800;
    int _clientHeight = 600;
};