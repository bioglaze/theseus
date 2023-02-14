#include "vec3.hpp"
#include <math.h>

void Vec3::Normalize()
{
    const float invLen = 1.0f / sqrtf( x * x + y * y + z * z );
    x *= invLen;
    y *= invLen;
    z *= invLen;
}

Vec3 Vec3::Normalized() const
{
    Vec3 res = *this;
    res.Normalize();
    return res;
}
