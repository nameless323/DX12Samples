//
// Just DX initialization.
//

#pragma once

#include <windows.h>

#include "../../../Core/Application.h"

namespace DX12Samples
{
class InitDX : public Application
{
public:
    InitDX(HINSTANCE hInstance);
    /**
     * \brief Preparing scene.
     */
    bool Init() override;
    ~InitDX() override;
    LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    int Run() override;

protected:
    /**
     * \brief Create render target and depth stencil heaps.
     */
    void CreateRtvAndDsvDescriptorHeaps() override;
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
};
}