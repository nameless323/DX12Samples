//
// Main application class. Creates and handles all Dx infrastructure.
//

#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "D3DUtil.h"
#include "GameTimer.h"

namespace DX12Samples
{
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
    /**
     * \brief Get if system supports 4x anti-aliasing.
     */
    bool Get4xMsaaState() const;
    /**
     * \brief Set antialiasing on/off. Internally recreates swap chain.
     */
    void Set4xMsaaState(bool value);
    /**
     * \brief Run application. This method handles windows message loop.
     */
    virtual int Run();
    /**
     * \brief Create system window and DX infrasturcture.
     */
    virtual bool Init();
    /**
     * \brief Windows message callback. See WINAPI.
     */
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    /**
     * \brief Create Render target and Depth stencil heaps. Override this if you need to add additional in this heaps.
     */
    virtual void CreateRtvAndDsvDescriptorHeaps();
    /**
     * \brief Recreates window size dependent resources.
     */
    virtual void OnResize();
    /**
     * \brief Calls when game needs to update logic (set buffers, update animations etc).
     */
    virtual void Update(const GameTimer& timer) abstract;
    /**
     * \brief Calls when system needs to redrow scene.
     */
    virtual void Draw(const GameTimer& timer) abstract;
    virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
    virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
    virtual void OnMouseMove(WPARAM btnState, int x, int y) {}
    /**
     * \brief Create system window.
     */
    bool InitMainWindow();
    /**
     * \brief Initialize DirectX infrastructure.
     */
    bool InitDirectX();
    /**
     * \brief Create command allocator, command queue and command list.
     */
    void CreateCommandObjects();
    /**
     * \brief Create swap chain.
     */
    void CreateSwapChain();
    /**
     * \brief Stop CPU executing while GPU doesn't finish exequte commands in queue.
     */
    void FlushCommandQueue();
    /**
     * \brief Get current back buffer resource.
     */
    ID3D12Resource* CurrentBackBuffer() const;
    /**
     * \brief Get current back buffer resource view.
     */
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    /**
     * \brief Get current depth stencil view.
     */
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
    /**
     * \brief Get FPS.
     */
    void CalculateFrameStats() const;
    /**
     * \brief Log all adapters (video cards) avaliable in system.
     */
    void LogAdapters();
    /**
     * \brief Log all avaliable monitors.
     */
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    /**
     * \brief Log all avaliable display modes (resolutions and refresh rate)
     */
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
}