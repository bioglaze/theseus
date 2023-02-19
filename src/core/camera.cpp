#include "camera.h"
#include "frustum.h"
#include "matrix.h"
#include "texture.h"

struct CameraImpl
{
    Matrix projection;
    float fovDegrees = 45;
    float aspect = 16.0f / 9.0f;
    float nearDepth = 0.1f;
    float farDepth = 400.0f;
    teProjectionType projectionType = teProjectionType::Perspective;
    teTexture2D color;
    teTexture2D colorResolve;
    teTexture2D depth;
    teTexture2D depthResolve;

    struct OrthoParams
    {
        float left = 0;
        float right = 100;
        float top = 0;
        float down = 100;
    } orthoParams;
};

constexpr unsigned MaxCameras = 10000;
CameraImpl cameras[ MaxCameras ];

teTexture2D& teCameraGetColorTexture( unsigned index )
{
    return cameras[ index ].color;
}

teTexture2D& teCameraGetColorResolveTexture( unsigned index )
{
    return cameras[ index ].colorResolve;
}

teTexture2D& teCameraGetDepthResolveTexture( unsigned index )
{
    return cameras[ index ].depthResolve;
}

teTexture2D& teCameraGetDepthTexture( unsigned index )
{
    return cameras[ index ].depth;
}

void teCameraSetProjectionType( unsigned index, teProjectionType projectionType )
{
    cameras[ index ].projectionType = projectionType;
}

void teCameraSetProjection( unsigned index, float fovDegrees, float aspect, float nearDepth, float farDepth )
{
    teAssert( index < MaxCameras );

    cameras[ index ].fovDegrees = fovDegrees;
    cameras[ index ].aspect = aspect;
    cameras[ index ].nearDepth = nearDepth;
    cameras[ index ].farDepth = farDepth;
    cameras[ index ].projection.MakeProjection( cameras[ index ].fovDegrees, cameras[ index ].aspect, cameras[ index ].nearDepth, cameras[ index ].farDepth );
    FrustumSetProjection( index, fovDegrees, aspect, nearDepth, farDepth );
}

void teCameraSetProjection( unsigned index, float left, float right, float bottom, float top, float aNear, float aFar )
{
    cameras[ index ].orthoParams.left = left;
    cameras[ index ].orthoParams.right = right;
    cameras[ index ].orthoParams.top = top;
    cameras[ index ].orthoParams.down = bottom;
    cameras[ index ].nearDepth = aNear;
    cameras[ index ].farDepth = aFar;
    cameras[ index ].projection.MakeProjection( left, right, bottom, top, aNear, aFar );
}

void teCameraGetProjection( unsigned index, float& outFovDegrees, float& outAspect, float& outNearDepth, float& outFarDepth )
{
    teAssert( index < MaxCameras );

    outFovDegrees = cameras[ index ].fovDegrees;
    outAspect = cameras[ index ].aspect;
    outNearDepth = cameras[ index ].nearDepth;
    outFarDepth = cameras[ index ].farDepth;
}

Matrix& teCameraGetProjection( unsigned index )
{
    teAssert( index < MaxCameras );

    return cameras[ index ].projection;
}
