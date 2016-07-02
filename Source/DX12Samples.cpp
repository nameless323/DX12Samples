#include "../Core/Application.h"
#include "Scenes/InitDX/InitDX.h"
#include "Scenes/Box/Box.h"
#include "Exercises/Ch6Ex.h"
#include "Scenes/Shapes/Shapes.h"
#include <strstream>
#include "Scenes/Waves/WavesScene.h"
#include "Scenes/LitColumns/LitColumns.h"
#include "Scenes/LitWaves/LitWaves.h"
#include "Scenes/Crate/Crate.h"
#include "Scenes/TexColumns/TexColumns.h"

Application* app;

void CreateScene(HINSTANCE hInstance)
{
    app = new TexColumns(hInstance);
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