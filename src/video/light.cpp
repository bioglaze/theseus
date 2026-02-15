#include "light.h"
#include "buffer.h"
#include "matrix.h"
#include "shader.h"
#include "vec3.h"

struct LightImpl
{
    unsigned tilerIndex = 0;
    float intensity = 1.0f;
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

void tePointLightSetParams( unsigned goIndex, float radius, const Vec3& color, float intensity )
{
    unsigned tilerIndex = pointLights[ goIndex ].tilerIndex;

    if (tilerIndex >= LightTiler::MaxLights)
    {
        return;
    }
    
    pointLights[ goIndex ].intensity = intensity;

    gLightTiler.pointLightCenterAndRadius[ tilerIndex ].w = radius;
    gLightTiler.pointLightColors[ tilerIndex ] = Vec4( color.x, color.y, color.z, 1 );    
}

void SetPointLightPosition( unsigned goIndex, const Vec3& positionWS )
{
    unsigned tilerIndex = pointLights[ goIndex ].tilerIndex;

    if (tilerIndex >= LightTiler::MaxLights)
    {
        return;
    }
    
    gLightTiler.pointLightCenterAndRadius[ tilerIndex ].x = positionWS.x;
    gLightTiler.pointLightCenterAndRadius[ tilerIndex ].y = positionWS.y;
    gLightTiler.pointLightCenterAndRadius[ tilerIndex ].z = positionWS.z;
}

float* tePointLightAccessRadius( unsigned goIndex )
{
    unsigned tilerIndex = pointLights[ goIndex ].tilerIndex;

    if (tilerIndex >= LightTiler::MaxLights)
    {
        return nullptr;
    }

    return &gLightTiler.pointLightCenterAndRadius[ tilerIndex ].w;
}

float* tePointLightAccessColor( unsigned goIndex )
{
    unsigned tilerIndex = pointLights[ goIndex ].tilerIndex;

    if (tilerIndex >= LightTiler::MaxLights)
    {
        return nullptr;
    }

    return &gLightTiler.pointLightColors[ tilerIndex ].x;
}

void tePointLightGetParams( unsigned goIndex, Vec3& outPosition, float& outRadius, Vec3& outColor, float& outIntensity )
{
    unsigned tilerIndex = pointLights[ goIndex ].tilerIndex;

    if (tilerIndex >= LightTiler::MaxLights)
    {
        return;
    }

    outPosition.x = gLightTiler.pointLightCenterAndRadius[ tilerIndex ].x;
    outPosition.y = gLightTiler.pointLightCenterAndRadius[ tilerIndex ].y;
    outPosition.z = gLightTiler.pointLightCenterAndRadius[ tilerIndex ].z;
    
    outRadius = gLightTiler.pointLightCenterAndRadius[ tilerIndex ].w;

    outIntensity = pointLights[ goIndex ].intensity;

    outColor.x = gLightTiler.pointLightColors[ tilerIndex ].x;
    outColor.y = gLightTiler.pointLightColors[ tilerIndex ].y;
    outColor.z = gLightTiler.pointLightColors[ tilerIndex ].z;
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
    gLightTiler.pointLightColorBuffer = CreateBuffer( LightTiler::MaxLights * 4 * sizeof( float ), "pointLightColorBuffer" );
    gLightTiler.pointLightCenterAndRadiusStagingBuffer = CreateStagingBuffer( LightTiler::MaxLights * 4 * sizeof( float ), "pointLightCenterAndRadiusStagingBuffer" );
    gLightTiler.pointLightColorStagingBuffer = CreateStagingBuffer( LightTiler::MaxLights * 4 * sizeof( float ), "pointLightColorStagingBuffer" );
}

void CullLights( const teShader& shader, const Matrix& localToView, const Matrix& viewToClip, unsigned widthPixels, unsigned heightPixels, unsigned depthNormalsTextureIndex )
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
    
    params.readTexture = depthNormalsTextureIndex;
    params.readBuffer = gLightTiler.pointLightCenterAndRadiusBuffer.index;
    params.writeBuffer = gLightTiler.lightIndexBuffer.index;
    
    teShaderDispatch( shader, GetLightTileCount( widthPixels ), GetLightTileCount( heightPixels ), 1, params, "Cull Lights" );
}
