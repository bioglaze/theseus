void FrustumSetProjection( int index, float fieldOfView, float aAspect, float aNear, float aFar );
void FrustumSetProjection( int index, float aLeft, float aRight, float aBottom, float aTop, float aNear, float aFar );
void UpdateFrustum( int index, const struct Vec3& cameraPosition, const Vec3& cameraDirection );
bool BoxInFrustum( int index, const Vec3& min, const Vec3& max );
