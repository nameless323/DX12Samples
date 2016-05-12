#pragma once
#include <windows.h>

class InitDX : public Application
{
public:
    InitDX(HINSTANCE hInstance);
    bool Init() override;
    ~InitDX() override;
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    int Run() override;
protected:
    void CreateRtvAndDsvDescriptorHeaps() override;
    void OnResize() override;
    void Update(const GameTimer& timer) override;
    void Draw(const GameTimer& timer) override;
    void OnMouseDown(WPARAM btnState, int x, int y) override;
    void OnMouseUp(WPARAM btnState, int x, int y) override;
    void OnMouseMove(WPARAM btnState, int x, int y) override;
};