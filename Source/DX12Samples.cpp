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
#include "Scenes/TexWaves/TexWaves.h"
#include "Scenes/Blending/Blending.h"
#include "Scenes/Stenciling/Stenciling.h"
#include "Scenes/Overdraw/OverdrawBlending.h"
#include "Scenes/Overdraw/OverdrawStenciling.h"
#include "Scenes/BilboardTrees/BilboardTrees.h"
#include "Scenes/Tesselation/BasicTesselation.h"
#include "Scenes/Tesselation/BezierPatch.h"
#include "Scenes/GeomCylinder/GeomCylinder.h"
#include "Scenes/Icosahedron/IcosahedronGeoTesselation.h"
#include "Scenes/GaussBlur/GaussBlur.h"
#include "Scenes/CSVectorAdd/CSVectorAdd.h"
#include "Scenes/CSVecLen/CSVecLen.h"
#include "Scenes/WavesCS/WavesCS.h"
#include "Scenes/DynamicIndexing/DynamicIndexing.h"
#include "Scenes/Instancing/Instancing.h"
#include "Scenes/Picking/Picking.h"
#include "Scenes/Cubemapping/Cubemapping.h"
#include "Scenes/Cubemapping/DynamicCubemap.h"
#include "Scenes/NormalMapping/NormalMapping.h"
#include "Scenes/Shadowmapping/Shadowmapping.h"
#include "Scenes/SSAO/SSAOScene.h"
#include "Scenes/RotationScene/RotationScene.h"
#include "Scenes/SkinnedAnimation/SkinnedAnimaiton.h"

Application* app;

void CreateScene(HINSTANCE hInstance)
{
    app = new SkinnedAnimation(hInstance);
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