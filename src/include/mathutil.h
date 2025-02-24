#pragma once

void ScreenPointToRay( int screenX, int screenY, float screenWidth, float screenHeight, unsigned cameraIndex, struct Vec3& outRayOrigin, Vec3& outRayTarget );
float IntersectRayAABB( const Vec3& origin, const Vec3& target, const Vec3& min, const Vec3& max );
