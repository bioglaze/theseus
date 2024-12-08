#include "frustum.h"
#include <math.h>
#include "vec3.h"

struct FrustumImpl
{
    void UpdateCornersAndCenters( const Vec3& cameraPosition, const Vec3& zAxis );
    
    // Near clipping plane coordinates
    Vec3 nearTopLeft;
    Vec3 nearTopRight;
    Vec3 nearBottomLeft;
    Vec3 nearBottomRight;
    
    // Far clipping plane coordinates.
    Vec3 farTopLeft;
    Vec3 farTopRight;
    Vec3 farBottomLeft;
    Vec3 farBottomRight;
    
    Vec3 nearCenter; // Near clipping plane center coordinate.
    Vec3 farCenter;  // Far clipping plane center coordinate.
    float zNear, zFar;
    float nearWidth, nearHeight;
    float farWidth, farHeight;
    
    struct Plane
    {
        float Distance( const Vec3& point ) const
        {
            return Vec3::Dot( normal, point ) + d;
        }
        
        void SetNormalAndPoint( const Vec3& aNormal, const Vec3& aPoint );
        
        void CalculateNormal();
        
        Vec3 a; // Point a on the plane.
        Vec3 b; // Point b on the plane.
        Vec3 c; // Point c on the plane.
        Vec3 normal; // Plane's normal pointing inside the frustum.
        float d;
    } planes[ 6 ]; // Clipping planes.
};

FrustumImpl frustums[ 10000 ];

void FrustumSetProjection( int index, float fieldOfView, float aAspect, float aNear, float aFar )
{
    FrustumImpl& fi = frustums[ index ];
    
    fi.zNear = aNear;
    fi.zFar  = aFar;
    
    // Computes width and height of the near and far plane sections.
    const float deg2rad = 3.14159265358979f / 180.0f;
    const float tang = tanf( deg2rad * fieldOfView * 0.5f );
    fi.nearHeight = fi.zNear * tang;
    fi.nearWidth  = fi.nearHeight * aAspect;
    fi.farHeight  = fi.zFar * tang;
    fi.farWidth   = fi.farHeight * aAspect;
}

void FrustumSetProjection( int index, float aLeft, float aRight, float aBottom, float aTop, float aNear, float aFar )
{
    FrustumImpl& fi = frustums[ index ];

    fi.zNear = aNear;
    fi.zFar  = aFar;
    
    fi.nearHeight = aTop - aBottom;
    fi.nearWidth  = aRight - aLeft;
    fi.farHeight  = fi.nearHeight;
    fi.farWidth   = fi.nearWidth;
}

void FrustumImpl::UpdateCornersAndCenters( const Vec3& cameraPosition, const Vec3& zAxis )
{
    const Vec3 up( 0, 1, 0 );
    const Vec3 xAxis = Vec3::Cross( up, zAxis ).Normalized();
    const Vec3 yAxis = Vec3::Cross( zAxis, xAxis ).Normalized();
    
    // Computes the centers of near and far planes.
    nearCenter = cameraPosition - zAxis * zNear;
    farCenter  = cameraPosition - zAxis * zFar;
    
    // Computes the near plane corners.
    nearTopLeft     = nearCenter + yAxis * nearHeight - xAxis * nearWidth;
    nearTopRight    = nearCenter + yAxis * nearHeight + xAxis * nearWidth;
    nearBottomLeft  = nearCenter - yAxis * nearHeight - xAxis * nearWidth;
    nearBottomRight = nearCenter - yAxis * nearHeight + xAxis * nearWidth;
    
    // Computes the far plane corners.
    farTopLeft     = farCenter + yAxis * farHeight - xAxis * farWidth;
    farTopRight    = farCenter + yAxis * farHeight + xAxis * farWidth;
    farBottomLeft  = farCenter - yAxis * farHeight - xAxis * farWidth;
    farBottomRight = farCenter - yAxis * farHeight + xAxis * farWidth;
}

void FrustumImpl::Plane::CalculateNormal()
{
    const Vec3 v1 = a - b;
    const Vec3 v2 = c - b;
    normal = Vec3::Cross( v2, v1 ).Normalized();
    d = -( Vec3::Dot( normal, b ) );
}

void FrustumImpl::Plane::SetNormalAndPoint( const Vec3& aNormal, const Vec3& aPoint )
{
    normal = aNormal.Normalized();
    d = -( Vec3::Dot( normal, aPoint ) );
}

void UpdateFrustum( int index, const Vec3& cameraPosition, const Vec3& cameraDirection )
{
    FrustumImpl& frustum = frustums[ index ];
    
    const Vec3 zAxis = cameraDirection;
    frustum.UpdateCornersAndCenters( cameraPosition, zAxis );
    
    enum FrustumPlane
    {
        Far = 0,
        Near,
        Bottom,
        Top,
        Left,
        Right
    };
    
    frustum.planes[ FrustumPlane::Top ].a = frustum.nearTopRight;
    frustum.planes[ FrustumPlane::Top ].b = frustum.nearTopLeft;
    frustum.planes[ FrustumPlane::Top ].c = frustum.farTopLeft;
    frustum.planes[ FrustumPlane::Top ].CalculateNormal();
    
    frustum.planes[ FrustumPlane::Bottom ].a = frustum.nearBottomLeft;
    frustum.planes[ FrustumPlane::Bottom ].b = frustum.nearBottomRight;
    frustum.planes[ FrustumPlane::Bottom ].c = frustum.farBottomRight;
    frustum.planes[ FrustumPlane::Bottom ].CalculateNormal();
    
    frustum.planes[ FrustumPlane::Left ].a = frustum.nearTopLeft;
    frustum.planes[ FrustumPlane::Left ].b = frustum.nearBottomLeft;
    frustum.planes[ FrustumPlane::Left ].c = frustum.farBottomLeft;
    frustum.planes[ FrustumPlane::Left ].CalculateNormal();
    
    frustum.planes[ FrustumPlane::Right ].a = frustum.nearBottomRight;
    frustum.planes[ FrustumPlane::Right ].b = frustum.nearTopRight;
    frustum.planes[ FrustumPlane::Right ].c = frustum.farBottomRight;
    frustum.planes[ FrustumPlane::Right ].CalculateNormal();
    
    frustum.planes[ FrustumPlane::Near ].SetNormalAndPoint( -zAxis, frustum.nearCenter );
    frustum.planes[ FrustumPlane::Far  ].SetNormalAndPoint(  zAxis, frustum.farCenter  );
}

// TODO: Rewrite using SIMD
bool BoxInFrustum( int index, const Vec3& min, const Vec3& max )
{
    bool result = true;
    
    Vec3 pos;
    
    // Determines positive and negative vertex in relation to the normal.
    for (unsigned p = 0; p < 6; ++p)
    {
        pos = min;
        
        if (frustums[ index ].planes[ p ].normal.x >= 0)
        {
            pos.x = max.x;
        }
        if (frustums[ index ].planes[ p ].normal.y >= 0)
        {
            pos.y = max.y;
        }
        if (frustums[ index ].planes[ p ].normal.z >= 0)
        {
            pos.z = max.z;
        }
        
        if (frustums[ index ].planes[ p ].Distance( pos ) < 0)
        {
            return false;
        }
    }
    
    return result;
}
