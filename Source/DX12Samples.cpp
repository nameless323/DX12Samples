#include "../Core/Application.h"
#include "Scenes/InitDX/InitDX.h"
#include "Scenes/Box/Box.h"
#include "Exercises/Ch6Ex.h"

Application* app;

void CreateScene(HINSTANCE hInstance)
{
    app = new Ch6Ex(hInstance);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
#if defined (DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    try
    {
        CreateScene(hInstance);
        if (!app->Init())
            return 0;
        return app->Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        delete app;
        app = nullptr;
        return 0;
    }
    delete app;
    app = nullptr;
}