#include "light.h"
#include "buffer.h"
#include "vec3.h"

struct LightImpl
{
    unsigned tilerIndex = 0;
} lights[ 10000 ];

unsigned gCurrentTilerIndex = 0;

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

void teAddLight( unsigned index )
{
    lights[ index ].tilerIndex = gCurrentTilerIndex++;
}

void tePointLightSetParams( unsigned goIndex, const Vec3& position, const Vec3& color )
{
    unsigned tilerIndex = lights[ goIndex ].tilerIndex;

    if (tilerIndex >= LightTiler::MaxLights)
    {
        return;
    }
    
    gLightTiler.pointLightCenterAndRadius[ tilerIndex ] = Vec4( position.x, position.y, position.z, 1 );
    gLightTiler.pointLightColors[ tilerIndex ] = Vec4( color.x, color.y, color.z, 1 );    
}

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
