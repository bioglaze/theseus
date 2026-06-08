#include "camera.h"
#include "frustum.h"
#include "matrix.h"
#include "quaternion.h"
#include "te_stdlib.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"

struct CameraImpl
{
    Matrix projection;
    float fovDegrees = 45;
    float aspect = 16.0f / 9.0f;
    float nearDepth = 0.1f;
    float farDepth = 400.0f;
    teProjectionType projectionType = teProjectionType::Perspective;
    teTexture2D color;
    teTexture2D depth;
    teTexture2D depthNormals;
    teClearFlag clearFlag = teClearFlag::DepthAndColor;
    Vec4 clearColor;

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

float teCameraGetFovDegrees( unsigned index )
{
    teAssert( index < MaxCameras );
    
    return cameras[ index ].fovDegrees;
}

float teCameraGetFar( unsigned index )
{
    teAssert( index < MaxCameras );
    
    return cameras[ index ].farDepth;
}

void teCameraSetClear( unsigned index, teClearFlag clearFlag, const Vec4& color )
{
    teAssert( index < MaxCameras );

    cameras[ index ].clearColor = color;
    cameras[ index ].clearFlag = clearFlag;
}

void teCameraGetClear( unsigned index, teClearFlag& outClearFlag, Vec4& outColor )
{
    teAssert( index < MaxCameras );

    outColor = cameras[ index ].clearColor;
    outClearFlag = cameras[ index ].clearFlag;
}

teTexture2D& teCameraGetColorTexture( unsigned index )
{
    teAssert( index < MaxCameras );
    return cameras[ index ].color;
}

teTexture2D& teCameraGetDepthTexture( unsigned index )
{
    teAssert( index < MaxCameras );
    return cameras[ index ].depth;
}

Vec3 teCameraGetScreenPoint( unsigned index, const Vec3& worldPoint, float viewWidth, float viewHeight )
{
    teAssert( index < MaxCameras );

    Matrix worldToClip;

    Matrix::Multiply( teTransformGetMatrix( index ), cameras[ index ].projection, worldToClip );
    Vec4 pos{ worldPoint.x, worldPoint.y, worldPoint.z, 1.0f };
    Matrix::TransformPoint( pos, worldToClip, pos );
    pos.x /= pos.w;
    pos.y /= pos.w;

    const float halfWidth = viewWidth * 0.5f;
    const float halfHeight = viewHeight * 0.5f;

    Vec3 out = Vec3( pos.x * halfWidth + halfWidth, -pos.y * halfHeight + halfHeight, pos.z );
#if API_METAL
    out.y = viewHeight - out.y;
#endif
    return out;
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

teTexture2D& teCameraGetDepthNormalsTexture( unsigned index )
{
    teAssert( index < MaxCameras );
    return cameras[ index ].depthNormals;
}
