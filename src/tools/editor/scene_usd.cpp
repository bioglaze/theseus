#ifdef USD_SUPPORT
#define NOMINMAX
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/xform.h>
#include <iostream>
#include "scene.h"

void LoadUsdScene( const char* path )
{
    pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateNew( path );
}
#endif
