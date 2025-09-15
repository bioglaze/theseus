#include "light.h"
#include "buffer.h"
#include "matrix.h"
#include "shader.h"
#include "vec3.h"

struct LightImpl
{
    unsigned tilerIndex = 0;
};

LightImpl pointLights[ 10000 ];
LightImpl spotLights[ 10000 ];

unsigned gCurrentPointTilerIndex = 0;
unsigned gCurrentSpotTilerIndex = 0;

struct LightTiler
{
    static constexpr int TileRes = 16;
    static constexpr unsigned MaxLightsPerTile = 544; // If this is changed, update lightculler shader.
    static constexpr unsigned MaxLights = 2048;

    teBuffer lightIndexBuffer;
    teBuffer pointLightCenterAndRadiusBuffer;
    teBuffer pointLightColorBuffer;
    teBuffer pointLightCenterAndRadiusStagingBuffer;
    teBuffer pointLightColorStagingBuffer;

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

teBuffer GetLightIndexBuffer()
{
    return gLightTiler.lightIndexBuffer;
}

void teAddPointLight( unsigned index )
{
    pointLights[ index ].tilerIndex = gCurrentPointTilerIndex++;
}

void teAddSpotLight( unsigned index )
{
    spotLights[ index ].tilerIndex = gCurrentSpotTilerIndex++;
}

unsigned GetPointLightCount()
{
    return gCurrentPointTilerIndex;
}

unsigned GetSpotLightCount()
{
    return gCurrentSpotTilerIndex;
}

void tePointLightSetParams( unsigned goIndex, const Vec3& position, float radius, const Vec3& color )
{
    unsigned tilerIndex = pointLights[ goIndex ].tilerIndex;

    if (tilerIndex >= LightTiler::MaxLights)
    {
        return;
    }
    
    gLightTiler.pointLightCenterAndRadius[ tilerIndex ] = Vec4( position.x, position.y, position.z, radius );
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
    gLightTiler.pointLightCenterAndRadiusStagingBuffer = CreateStagingBuffer( LightTiler::MaxLights * 4 * sizeof( float ), "pointLightCenterAndRadiusStagingBuffer" );
    gLightTiler.pointLightColorStagingBuffer = CreateStagingBuffer( LightTiler::MaxLights * 4 * sizeof( float ), "pointLightColorStagingBuffer" );
}

void CullLights( const teShader& shader, const Matrix& localToView, const Matrix& viewToClip, unsigned widthPixels, unsigned heightPixels )
{
    UpdateStagingBuffer( gLightTiler.pointLightCenterAndRadiusStagingBuffer, gLightTiler.pointLightCenterAndRadius, LightTiler::MaxLights * 4 * sizeof( float ), 0 );
    UpdateStagingBuffer( gLightTiler.pointLightColorStagingBuffer, gLightTiler.pointLightColors, LightTiler::MaxLights * 4 * sizeof( float ), 0 );

    CopyBuffer( gLightTiler.pointLightCenterAndRadiusStagingBuffer, gLightTiler.pointLightCenterAndRadiusBuffer );
    CopyBuffer( gLightTiler.pointLightColorStagingBuffer, gLightTiler.pointLightColorBuffer );

    ShaderParams params = {};

    Matrix clipToView;
    Matrix::Invert( viewToClip, clipToView );

    for (unsigned i = 0; i < 16; ++i)
    {
        params.clipToView[ i ] = clipToView.m[ i ];
        params.localToView[ i ] = localToView.m[ i ];
    }

    params.tilesXY[ 0 ] = (float)widthPixels;
    params.tilesXY[ 1 ] = (float)heightPixels;
    
    params.readBuffer = gLightTiler.pointLightCenterAndRadiusBuffer.index;
    params.writeBuffer = gLightTiler.lightIndexBuffer.index;
    
    teShaderDispatch( shader, GetLightTileCount( widthPixels ), GetLightTileCount( heightPixels ), 1, params, "Cull Lights" );
}
