#include "light.h"
#include "buffer.h"
#include "matrix.h"
#include "shader.h"
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

teBuffer GetPointLightCenterAndRadiusBuffer()
{
    return gLightTiler.pointLightCenterAndRadiusBuffer;
}

teBuffer GetPointLightColorBuffer()
{
    return gLightTiler.pointLightColorBuffer;
}

void teAddLight( unsigned index )
{
    lights[ index ].tilerIndex = gCurrentTilerIndex++;
}

unsigned GetPointLightCount()
{
    return gCurrentTilerIndex;
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

unsigned GetMaxLightsPerTile( unsigned height )
{
    constexpr unsigned AdjustmentMultipier = 32; // FIXME: Should this be equal to tile size?

    // I haven't tested at greater than 1080p, so cap it
    const unsigned uHeight = (height > 1080) ? 1080 : height;

    // adjust max lights per tile down as height increases
    return (LightTiler::MaxLightsPerTile - (AdjustmentMultipier * (uHeight / 120)));
}

unsigned GetLightTileCount( unsigned renderTargetWidthOrHeight )
{
    return (unsigned)((renderTargetWidthOrHeight + LightTiler::TileRes - 1) / (float)LightTiler::TileRes);
}

void InitLightTiler( unsigned widthPixels, unsigned heightPixels )
{
    const unsigned tileCount = GetLightTileCount( widthPixels ) * GetLightTileCount( heightPixels );
    const unsigned maxLightsPerTile = GetMaxLightsPerTile( heightPixels );
    
    gLightTiler.lightIndexBuffer = CreateBuffer( maxLightsPerTile * tileCount * sizeof( unsigned ), "lightIndexBuffer" );
    gLightTiler.pointLightCenterAndRadiusBuffer = CreateBuffer( LightTiler::MaxLights * 4 * sizeof( float ), "pointLightCenterAndRadiusBuffer" );
    gLightTiler.pointLightColorBuffer = CreateBuffer( LightTiler::MaxLights * 4 * sizeof( float ), "pointLightColorAndRadiusBuffer" );
}

void CullLights( const teShader& shader, const Matrix& localToClip, const Matrix& localToView, const Matrix& viewToClip, unsigned widthPixels, unsigned heightPixels )
{
    // TODO: remember to update the light position/radius etc. before calling this.
    ShaderParams params = {};

    Matrix clipToView;
    Matrix::Invert( viewToClip, clipToView );

    for (unsigned i = 0; i < 16; ++i)
    {
        params.clipToView[ i ] = clipToView.m[ i ];
        params.localToView[ i ] = localToView.m[ i ];
    }

    params.tilesXY[ 0 ] = (float)widthPixels; // FIXME: In Aether3D this was GetLightTileCount( width ) but that same calculation is done in shader, so seems like an Aether bug!
    params.tilesXY[ 1 ] = (float)heightPixels;
    teShaderDispatch( shader, GetLightTileCount( widthPixels ), GetLightTileCount( heightPixels ), 1, params, "Cull Lights" );
}
