#include "light.h"
#include "buffer.h"
#include "vec3.h"

struct LightTiler
{
    static constexpr int TileRes = 16;
    static constexpr unsigned MaxLightsPerTile = 544;
    static constexpr unsigned MaxLights = 2048;

    teBuffer lightIndexBuffer;
    teBuffer pointLightCenterAndRadiusBuffer;
    teBuffer pointLightColorBuffer;

    Vec4 pointLightCenterAndRadius[ MaxLights ];
    Vec4 pointLightColors[ MaxLights ];
} gLightTiler;

unsigned GetLightTileCount( unsigned renderTargetWidthOrHeight )
{
    return (unsigned)((renderTargetWidthOrHeight + LightTiler::TileRes - 1) / (float)LightTiler::TileRes);
}

void InitLightTiler( unsigned widthPixels, unsigned heightPixels )
{
    const unsigned numTiles = GetLightTileCount( widthPixels ) * GetLightTileCount( heightPixels );

    gLightTiler.pointLightCenterAndRadiusBuffer = CreateBuffer( LightTiler::MaxLights * 4 * sizeof( float ), "pointLightCenterAndRadiusBuffer" );
    gLightTiler.pointLightColorBuffer = CreateBuffer( LightTiler::MaxLights * 4 * sizeof( float ), "pointLightColorAndRadiusBuffer" );
}
