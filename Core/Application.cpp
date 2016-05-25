#include "Application.h"
#include <WindowsX.h>

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return Application::GetApplication()->MsgProc(hwnd, msg, wParam, lParam);
}

Application* Application::_app = nullptr;
Application* Application::GetApplication()
{
    return _app;
}

Application::Application(HINSTANCE hInstance) : _hAppInstance(hInstance)
{
    assert(_app == nullptr);
    _app = this;
}

Application::~Application()
{
    if (_device != nullptr)
        FlushCommandQueue();
}

HINSTANCE Application::AppInstance() const
{
    return _hAppInstance;
}

HWND Application::MainWindow() const
{
    return _hMainWindow;
}

float Application::AspectRatio() const
{
    return static_cast<float>(_clientWidth) / _clientHeight;
}

bool Application::Get4xMsaaState() const
{
    return _4xMsaa;
}

void Application::Set4xMsaaState(bool value)
{
    if (_4xMsaa != value)
    {
        _4xMsaa = value;
        CreateSwapChain();
        OnResize();
    }
}

int Application::Run()
{
    MSG msg = { 0 };
    _timer.Reset();

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            _timer.Tick();
            if (!_isPaused)
            {
                CalculateFrameStats();
                Update(_timer);
                Draw(_timer);
            }
            else
            {
                Sleep(100);
            }
        }
    }
    return (int)msg.wParam;
}

bool Application::Init()
{
    if (!InitMainWindow())
        return false;
    if (!InitDirectX())
        return false;
    OnResize();
    return true;
}

void Application::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = _swapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(_rtvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeap.GetAddressOf())));
}

void Application::OnResize()
{
    assert(_device);
    assert(_swapChain);
    assert(_commandAllocator);
    FlushCommandQueue();

    ThrowIfFailed(_commandList->Reset(_commandAllocator.Get(), nullptr));

    for (int i = 0; i < _swapChainBufferCount; i++)
        _swapChainBuffer[i].Reset();
    _depthStencilBuffer.Reset();

    ThrowIfFailed(_swapChain->ResizeBuffers(_swapChainBufferCount, _clientWidth, _clientHeight, _backBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));
    _currBackBuffer = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < _swapChainBufferCount; i++)
    {
        ThrowIfFailed(_swapChain->GetBuffer(i, IID_PPV_ARGS(&_swapChainBuffer[i])));
        _device->CreateRenderTargetView(_swapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
        rtvHeapHandle.Offset(1, _rtvDescriptorSize);
    }
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = _clientWidth;
    depthStencilDesc.Height = _clientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = _dsvFormat;
    depthStencilDesc.SampleDesc.Count = _4xMsaa ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = _4xMsaa ? (_4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = _dsvFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    ThrowIfFailed(_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &depthStencilDesc, D3D12_RESOURCE_STATE_COMMON, &optClear, IID_PPV_ARGS(_depthStencilBuffer.GetAddressOf())));
    _device->CreateDepthStencilView(_depthStencilBuffer.Get(), nullptr, DepthStencilView());

    _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
    ThrowIfFailed(_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { _commandList.Get() };
    _commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();

    _screenViewport.TopLeftX = 0;
    _screenViewport.TopLeftY = 0;
    _screenViewport.Width = static_cast<float>(_clientWidth);
    _screenViewport.Height = static_cast<float>(_clientHeight);
    _screenViewport.MinDepth = 0.0f;
    _screenViewport.MaxDepth = 1.0f;

    _scissorRect = { 0, 0, _clientWidth, _clientHeight };
}

LRESULT Application::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        // WM_ACTIVATE is sent when the window is activated or deactivated.  
        // We pause the game when the window is deactivated and unpause it 
        // when it becomes active.  
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            _isPaused = true;
            _timer.Stop();
        }
        else
        {
            _isPaused = false;
            _timer.Start();
        }
        return 0;

        // WM_SIZE is sent when the user resizes the window.  
    case WM_SIZE:
        // Save the new client area dimensions.
        _clientWidth = LOWORD(lParam);
        _clientHeight = HIWORD(lParam);
        if (_device)
        {
            if (wParam == SIZE_MINIMIZED)
            {
                _isPaused = true;
                _isMinimized = true;
                _isMaximized = false;
            }
            else if (wParam == SIZE_MAXIMIZED)
            {
                _isPaused = false;
                _isMinimized = false;
                _isMaximized = true;
                OnResize();
            }
            else if (wParam == SIZE_RESTORED)
            {

                // Restoring from minimized state?
                if (_isMinimized)
                {
                    _isPaused = false;
                    _isMinimized = false;
                    OnResize();
                }

                // Restoring from maximized state?
                else if (_isMaximized)
                {
                    _isPaused = false;
                    _isMaximized = false;
                    OnResize();
                }
                else if (_isResizing)
                {
                    // If user is dragging the resize bars, we do not resize 
                    // the buffers here because as the user continuously 
                    // drags the resize bars, a stream of WM_SIZE messages are
                    // sent to the window, and it would be pointless (and slow)
                    // to resize for each WM_SIZE message received from dragging
                    // the resize bars.  So instead, we reset after the user is 
                    // done resizing the window and releases the resize bars, which 
                    // sends a WM_EXITSIZEMOVE message.
                }
                else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
                {
                    OnResize();
                }
            }
        }
        return 0;

        // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
    case WM_ENTERSIZEMOVE:
        _isPaused = true;
        _isResizing = true;
        _timer.Stop();
        return 0;

        // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
        // Here we reset everything based on the new window dimensions.
    case WM_EXITSIZEMOVE:
        _isPaused = false;
        _isResizing = false;
        _timer.Start();
        OnResize();
        return 0;

        // WM_DESTROY is sent when the window is being destroyed.
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

        // The WM_MENUCHAR message is sent when a menu is active and the user presses 
        // a key that does not correspond to any mnemonic or accelerator key. 
    case WM_MENUCHAR:
        // Don't beep when we alt-enter.
        return MAKELRESULT(0, MNC_CLOSE);

        // Catch this message so to prevent the window from becoming too small.
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
        ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
        return 0;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_KEYUP:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        else if ((int)wParam == VK_F2)
            Set4xMsaaState(!_4xMsaa);

        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Application::InitMainWindow()
{
    WNDCLASS wc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = _hAppInstance;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = L"MainWnd";

    if (!RegisterClass(&wc))
    {
        MessageBox(0, L"RegisterClass Failed", 0, 0);
        return false;
    }
    RECT R = { 0, 0, _clientWidth, _clientHeight };
    AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
    int width = R.right - R.left;
    int height = R.bottom - R.top;

    _hMainWindow = CreateWindow(L"MainWnd", _mainWindowCaption.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, _hAppInstance, 0);
    if (!_hMainWindow)
    {
        MessageBox(0, L"CreateWindow Failed", 0, 0);
        return false;
    }
    ShowWindow(_hMainWindow, SW_SHOW);
    UpdateWindow(_hMainWindow);

    return true;
}

bool Application::InitDirectX()
{
    #if defined(DEBUG) || defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debugController;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
        debugController->EnableDebugLayer();
    }
    #endif
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory)));
    HRESULT hardwareResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device));
    if (FAILED(hardwareResult))
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
        ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device)));
    }
    ThrowIfFailed(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence)));
    _rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    _dsvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    _cbvSrvUavDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
    msQualityLevels.Format = _backBufferFormat;
    msQualityLevels.SampleCount = 4;
    msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels.NumQualityLevels = 0;
    ThrowIfFailed(_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));
    _4xMsaaQuality = msQualityLevels.NumQualityLevels;
    assert(_4xMsaaQuality > 0 && "Unexpected MSAA quality level");
#ifdef _DEBUG
    LogAdapters();
#endif
    CreateCommandObjects();
    CreateSwapChain();
    CreateRtvAndDsvDescriptorHeaps();
    return true;
}

void Application::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_commandQueue)));

    ThrowIfFailed(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_commandAllocator.GetAddressOf())));
    
    ThrowIfFailed(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.Get(), nullptr, IID_PPV_ARGS(_commandList.GetAddressOf())));

    _commandList->Close();
}

void Application::CreateSwapChain()
{
    _swapChain.Reset();

    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width = _clientWidth;
    sd.BufferDesc.Height = _clientHeight;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = _backBufferFormat;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count = _4xMsaa ? 4 : 1;
    sd.SampleDesc.Quality = _4xMsaa ? (_4xMsaaQuality - 1) : 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = _swapChainBufferCount;
    sd.OutputWindow = _hMainWindow;
    sd.Windowed = true;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    ThrowIfFailed(_dxgiFactory->CreateSwapChain(_commandQueue.Get(), &sd, _swapChain.GetAddressOf()));
}

void Application::FlushCommandQueue()
{
    _currentFence++;
    ThrowIfFailed(_commandQueue->Signal(_fence.Get(), _currentFence));

    if (_fence->GetCompletedValue() < _currentFence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(_fence->SetEventOnCompletion(_currentFence, eventHandle));

        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

ID3D12Resource* Application::CurrentBackBuffer() const
{
    return _swapChainBuffer[_currBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE Application::CurrentBackBufferView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(_rtvHeap->GetCPUDescriptorHandleForHeapStart(), _currBackBuffer, _rtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE Application::DepthStencilView() const
{
    return _dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void Application::CalculateFrameStats() const
{
    static int frameCnt = 0;
    static float timeElapsed = 0.0f;
    frameCnt++;

    if (_timer.TotalTime() - timeElapsed >= 1.0f)
    {
        float fps = (float)frameCnt;
        float mspf = 1000.0f / fps;

        wstring fpsStr = to_wstring(fps);
        wstring mspfStr = to_wstring(mspf);

        wstring windowText = _mainWindowCaption + L"  fps: " + fpsStr + L"  mspf: " + mspfStr;
        SetWindowText(_hMainWindow, windowText.c_str());

        frameCnt = 0;
        timeElapsed += 1.0f;
    }
}

void Application::LogAdapters()
{
    UINT i = 0;
    IDXGIAdapter* adapter = nullptr;
    vector<IDXGIAdapter*> adapterList;
    while (_dxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        wstring text = L"***ADAPTER: ";
        text += desc.Description;
        text += L"\n";
        OutputDebugString(text.c_str());
        adapterList.push_back(adapter);
        ++i;
    }
    for (size_t j = 0; j < adapterList.size(); j++)
    {
        LogAdapterOutputs(adapterList[j]);
        ReleaseCom(adapterList[j]);
    }
}

void Application::LogAdapterOutputs(IDXGIAdapter* adapter)
{
    UINT i = 0;
    IDXGIOutput* output = nullptr;
    while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);

        wstring text = L"***OUTPUT: ";
        text += desc.DeviceName;
        text += L"\n";
        OutputDebugString(text.c_str());
        LogOutputDisplayModes(output, _backBufferFormat);
        ReleaseCom(output);
        ++i;
    }
}

void Application::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
    UINT count = 0;
    UINT flags = 0;

    output->GetDisplayModeList(format, flags, &count, nullptr);
    vector<DXGI_MODE_DESC> modeList(count);
    output->GetDisplayModeList(format, flags, &count, &modeList[0]);
    for (auto& x : modeList)
    {
        UINT n = x.RefreshRate.Numerator;
        UINT d = x.RefreshRate.Denominator;
        wstring text =
            L"Width = " + to_wstring(x.Width) + L"  " +
            L"Height = " + to_wstring(x.Height) + L"  " +
            L"Refresh = " + to_wstring(n) + L"/" + to_wstring(d) +
            L"\n";

        OutputDebugString(text.c_str());
    }
}
