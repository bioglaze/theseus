#pragma once

void ScreenPointToRay( int screenX, int screenY, float screenWidth, float screenHeight, unsigned cameraIndex, struct Vec3& outRayOrigin, Vec3& outRayTarget );
float IntersectRayAABB( const Vec3& origin, const Vec3& target, const Vec3& min, const Vec3& max );
void GetMinMax( const Vec3* points, int count, Vec3& outMin, Vec3& outMax );
void teGetCorners( const Vec3& min, const Vec3& max, Vec3 outCorners[ 8 ] );