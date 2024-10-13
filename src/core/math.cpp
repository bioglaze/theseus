#include "matrix.h"
#include "quaternion.h"
#include "te_stdlib.h"
#include "vec3.h"
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

void Vec4::Normalize()
{
    const float invLen = 1.0f / sqrtf( x * x + y * y + z * z );
    x *= invLen;
    y *= invLen;
    z *= invLen;
}

#ifdef SIMD_SSE3
#include <pmmintrin.h>

void Matrix::Multiply( const Matrix& ma, const Matrix& mb, Matrix& out )
{
    Matrix result;

    for (unsigned i = 0; i < 16; i += 4)
    {
        // unroll the first step of the loop to avoid having to initialize r_line to zero
        __m128 a_line = _mm_load_ps( mb.m );
        __m128 b_line = _mm_set1_ps( ma.m[ i ] );
        __m128 r_line = _mm_mul_ps( a_line, b_line );

        for (int j = 1; j < 4; ++j)
        {
            a_line = _mm_load_ps( &mb.m[ j * 4 ] );
            b_line = _mm_set1_ps( ma.m[ i + j ] );
            r_line = _mm_add_ps( _mm_mul_ps( a_line, b_line ), r_line );
        }

        _mm_store_ps( &result.m[ i ], r_line );
    }

    out = result;
}

void Matrix::TransformPoint( const Vec3& vec, const Matrix& mat, Vec3& out )
{
    alignas(16) float v4[ 4 ] = { vec.x, vec.y, vec.z, 1 };

    __m128 vec4 = _mm_load_ps( v4 );
    __m128 vTempX = _mm_shuffle_ps( vec4, vec4, _MM_SHUFFLE( 0, 0, 0, 0 ) );
    __m128 vTempY = _mm_shuffle_ps( vec4, vec4, _MM_SHUFFLE( 1, 1, 1, 1 ) );
    __m128 vTempZ = _mm_shuffle_ps( vec4, vec4, _MM_SHUFFLE( 2, 2, 2, 2 ) );
    __m128 vTempW = _mm_shuffle_ps( vec4, vec4, _MM_SHUFFLE( 3, 3, 3, 3 ) );

    __m128 nrow1 = _mm_load_ps( &mat.m[ 0 ] );
    __m128 nrow2 = _mm_load_ps( &mat.m[ 4 ] );
    __m128 nrow3 = _mm_load_ps( &mat.m[ 8 ] );
    __m128 nrow4 = _mm_load_ps( &mat.m[ 12 ] );

    vTempX = _mm_mul_ps( vTempX, nrow1 );
    vTempY = _mm_mul_ps( vTempY, nrow2 );
    vTempZ = _mm_mul_ps( vTempZ, nrow3 );
    vTempW = _mm_mul_ps( vTempW, nrow4 );

    vTempX = _mm_add_ps( vTempX, vTempY );
    vTempZ = _mm_add_ps( vTempZ, vTempW );
    vTempX = _mm_add_ps( vTempX, vTempZ );
    alignas(16) float tmp[ 4 ];
    _mm_store_ps( tmp, vTempX );

    out.x = tmp[ 0 ];
    out.y = tmp[ 1 ];
    out.z = tmp[ 2 ];
}
#elif SIMD_NEON
#include <arm_neon.h>

void Matrix::Multiply( const Matrix& ma, const Matrix& mb, Matrix& out )
{
    Matrix result;
    Matrix matA = ma;
    Matrix matB = mb;
    
    const float* a = matA.m;
    const float* b = matB.m;
    float32x4_t a_line, b_line, r_line;
    
    for (int i = 0; i < 16; i += 4)
    {
        // unroll the first step of the loop to avoid having to initialize r_line to zero
        a_line = vld1q_f32( b );
        b_line = vld1q_dup_f32( &a[ i ] );
        r_line = vmulq_f32( a_line, b_line );
        
        for (int j = 1; j < 4; j++)
        {
            a_line = vld1q_f32( &b[ j * 4 ] );
            b_line = vdupq_n_f32(  a[ i + j ] );
            r_line = vaddq_f32(vmulq_f32( a_line, b_line ), r_line);
        }
        
        vst1q_f32( &result.m[ i ], r_line );
    }
    
    out = result;
}

void Matrix::TransformPoint( const Vec3& point, const Matrix& mat, Vec3& out )
{
    Matrix trans;
    mat.Transpose( trans );
    
    alignas( 16 ) float vec[ 4 ] = { point.x, point.y, point.z, 1 };
    
    float32x4x4_t matt = vld4q_f32( trans.m );
    float32x4_t vec4 = vld1q_f32( vec );

    float32x4_t result = vmulq_lane_f32( matt.val[ 0 ], vget_low_f32( vec4 ), 0 );
    result = vmlaq_lane_f32( result, matt.val[ 1 ], vget_low_f32( vec4 ), 1 );
    result = vmlaq_lane_f32( result, matt.val[ 2 ], vget_high_f32( vec4 ), 0 );
    result = vmlaq_lane_f32( result, matt.val[ 3 ], vget_high_f32( vec4 ), 1 );

    alignas( 16 ) float res[ 4 ];
    vst1q_f32( res, result );
    
    out.x = res[ 0 ];
    out.y = res[ 1 ];
    out.z = res[ 2 ];
}

#endif

void Matrix::InitFrom( const float* mat )
{
#ifdef SIMD_SSE3
    __m128 row1 = _mm_load_ps( &mat[ 0 ] );
    __m128 row2 = _mm_load_ps( &mat[ 4 ] );
    __m128 row3 = _mm_load_ps( &mat[ 8 ] );
    __m128 row4 = _mm_load_ps( &mat[ 12 ] );
    _mm_store_ps( &m[ 0 ], row1 );
    _mm_store_ps( &m[ 4 ], row2 );
    _mm_store_ps( &m[ 8 ], row3 );
    _mm_store_ps( &m[ 12 ], row4 );
#else
    for (unsigned i = 0; i < 16; ++i)
    {
        m[ i ] = mat[ i ];
    }
#endif
}

void Matrix::MakeLookAtLH( const Vec3& eye, const Vec3& center, const Vec3& up )
{
    const Vec3 zAxis = (center - eye).Normalized();
    const Vec3 xAxis = Vec3::Cross( up, zAxis ).Normalized();
    const Vec3 yAxis = Vec3::Cross( zAxis, xAxis ).Normalized();

    m[ 0 ] = xAxis.x; m[ 1 ] = xAxis.y; m[ 2 ] = xAxis.z; m[ 3 ] = -Vec3::Dot( xAxis, eye );
    m[ 4 ] = yAxis.x; m[ 5 ] = yAxis.y; m[ 6 ] = yAxis.z; m[ 7 ] = -Vec3::Dot( yAxis, eye );
    m[ 8 ] = zAxis.x; m[ 9 ] = zAxis.y; m[ 10 ] = zAxis.z; m[ 11 ] = -Vec3::Dot( zAxis, eye );
    m[ 12 ] = 0; m[ 13 ] = 0; m[ 14 ] = 0; m[ 15 ] = 1;
}

void Matrix::Invert( const Matrix& matrix, Matrix& out )
{
    float invTrans[ 16 ];
    InverseTranspose( &matrix.m[ 0 ], &invTrans[ 0 ] );
    Matrix iTrans;
    iTrans.InitFrom( &invTrans[ 0 ] );
    iTrans.Transpose( out );
}

void Matrix::InverseTranspose( const float m[ 16 ], float* out )
{
    float tmp[ 12 ];

    // Calculates pairs for first 8 elements (cofactors)
    tmp[ 0 ] = m[ 10 ] * m[ 15 ];
    tmp[ 1 ] = m[ 11 ] * m[ 14 ];
    tmp[ 2 ] = m[ 9 ] * m[ 15 ];
    tmp[ 3 ] = m[ 11 ] * m[ 13 ];
    tmp[ 4 ] = m[ 9 ] * m[ 14 ];
    tmp[ 5 ] = m[ 10 ] * m[ 13 ];
    tmp[ 6 ] = m[ 8 ] * m[ 15 ];
    tmp[ 7 ] = m[ 11 ] * m[ 12 ];
    tmp[ 8 ] = m[ 8 ] * m[ 14 ];
    tmp[ 9 ] = m[ 10 ] * m[ 12 ];
    tmp[ 10 ] = m[ 8 ] * m[ 13 ];
    tmp[ 11 ] = m[ 9 ] * m[ 12 ];

    // Calculates first 8 elements (cofactors)
    out[ 0 ] = tmp[ 0 ] * m[ 5 ] + tmp[ 3 ] * m[ 6 ] + tmp[ 4 ] * m[ 7 ] - tmp[ 1 ] * m[ 5 ] - tmp[ 2 ] * m[ 6 ] - tmp[ 5 ] * m[ 7 ];
    out[ 1 ] = tmp[ 1 ] * m[ 4 ] + tmp[ 6 ] * m[ 6 ] + tmp[ 9 ] * m[ 7 ] - tmp[ 0 ] * m[ 4 ] - tmp[ 7 ] * m[ 6 ] - tmp[ 8 ] * m[ 7 ];
    out[ 2 ] = tmp[ 2 ] * m[ 4 ] + tmp[ 7 ] * m[ 5 ] + tmp[ 10 ] * m[ 7 ] - tmp[ 3 ] * m[ 4 ] - tmp[ 6 ] * m[ 5 ] - tmp[ 11 ] * m[ 7 ];
    out[ 3 ] = tmp[ 5 ] * m[ 4 ] + tmp[ 8 ] * m[ 5 ] + tmp[ 11 ] * m[ 6 ] - tmp[ 4 ] * m[ 4 ] - tmp[ 9 ] * m[ 5 ] - tmp[ 10 ] * m[ 6 ];
    out[ 4 ] = tmp[ 1 ] * m[ 1 ] + tmp[ 2 ] * m[ 2 ] + tmp[ 5 ] * m[ 3 ] - tmp[ 0 ] * m[ 1 ] - tmp[ 3 ] * m[ 2 ] - tmp[ 4 ] * m[ 3 ];
    out[ 5 ] = tmp[ 0 ] * m[ 0 ] + tmp[ 7 ] * m[ 2 ] + tmp[ 8 ] * m[ 3 ] - tmp[ 1 ] * m[ 0 ] - tmp[ 6 ] * m[ 2 ] - tmp[ 9 ] * m[ 3 ];
    out[ 6 ] = tmp[ 3 ] * m[ 0 ] + tmp[ 6 ] * m[ 1 ] + tmp[ 11 ] * m[ 3 ] - tmp[ 2 ] * m[ 0 ] - tmp[ 7 ] * m[ 1 ] - tmp[ 10 ] * m[ 3 ];
    out[ 7 ] = tmp[ 4 ] * m[ 0 ] + tmp[ 9 ] * m[ 1 ] + tmp[ 10 ] * m[ 2 ] - tmp[ 5 ] * m[ 0 ] - tmp[ 8 ] * m[ 1 ] - tmp[ 11 ] * m[ 2 ];

    // Calculates pairs for second 8 elements (cofactors)
    tmp[ 0 ] = m[ 2 ] * m[ 7 ];
    tmp[ 1 ] = m[ 3 ] * m[ 6 ];
    tmp[ 2 ] = m[ 1 ] * m[ 7 ];
    tmp[ 3 ] = m[ 3 ] * m[ 5 ];
    tmp[ 4 ] = m[ 1 ] * m[ 6 ];
    tmp[ 5 ] = m[ 2 ] * m[ 5 ];
    tmp[ 6 ] = m[ 0 ] * m[ 7 ];
    tmp[ 7 ] = m[ 3 ] * m[ 4 ];
    tmp[ 8 ] = m[ 0 ] * m[ 6 ];
    tmp[ 9 ] = m[ 2 ] * m[ 4 ];
    tmp[ 10 ] = m[ 0 ] * m[ 5 ];
    tmp[ 11 ] = m[ 1 ] * m[ 4 ];

    // Calculates second 8 elements (cofactors)
    out[ 8 ] = tmp[ 0 ] * m[ 13 ] + tmp[ 3 ] * m[ 14 ] + tmp[ 4 ] * m[ 15 ]
        - tmp[ 1 ] * m[ 13 ] - tmp[ 2 ] * m[ 14 ] - tmp[ 5 ] * m[ 15 ];

    out[ 9 ] = tmp[ 1 ] * m[ 12 ] + tmp[ 6 ] * m[ 14 ] + tmp[ 9 ] * m[ 15 ]
        - tmp[ 0 ] * m[ 12 ] - tmp[ 7 ] * m[ 14 ] - tmp[ 8 ] * m[ 15 ];

    out[ 10 ] = tmp[ 2 ] * m[ 12 ] + tmp[ 7 ] * m[ 13 ] + tmp[ 10 ] * m[ 15 ]
        - tmp[ 3 ] * m[ 12 ] - tmp[ 6 ] * m[ 13 ] - tmp[ 11 ] * m[ 15 ];

    out[ 11 ] = tmp[ 5 ] * m[ 12 ] + tmp[ 8 ] * m[ 13 ] + tmp[ 11 ] * m[ 14 ]
        - tmp[ 4 ] * m[ 12 ] - tmp[ 9 ] * m[ 13 ] - tmp[ 10 ] * m[ 14 ];

    out[ 12 ] = tmp[ 2 ] * m[ 10 ] + tmp[ 5 ] * m[ 11 ] + tmp[ 1 ] * m[ 9 ]
        - tmp[ 4 ] * m[ 11 ] - tmp[ 0 ] * m[ 9 ] - tmp[ 3 ] * m[ 10 ];

    out[ 13 ] = tmp[ 8 ] * m[ 11 ] + tmp[ 0 ] * m[ 8 ] + tmp[ 7 ] * m[ 10 ]
        - tmp[ 6 ] * m[ 10 ] - tmp[ 9 ] * m[ 11 ] - tmp[ 1 ] * m[ 8 ];

    out[ 14 ] = tmp[ 6 ] * m[ 9 ] + tmp[ 11 ] * m[ 11 ] + tmp[ 3 ] * m[ 8 ]
        - tmp[ 10 ] * m[ 11 ] - tmp[ 2 ] * m[ 8 ] - tmp[ 7 ] * m[ 9 ];

    out[ 15 ] = tmp[ 10 ] * m[ 10 ] + tmp[ 4 ] * m[ 8 ] + tmp[ 9 ] * m[ 9 ]
        - tmp[ 8 ] * m[ 9 ] - tmp[ 11 ] * m[ 10 ] - tmp[ 5 ] * m[ 8 ];

    // Calculates the determinant.
    const float det = m[ 0 ] * out[ 0 ] + m[ 1 ] * out[ 1 ] + m[ 2 ] * out[ 2 ] + m[ 3 ] * out[ 3 ];
    const float acceptableDelta = 0.0001f;

    if (fabs( det ) < acceptableDelta)
    {
        Matrix identity;
        teMemcpy( out, &identity.m[ 0 ], sizeof( Matrix ) );
        return;
    }
    for (int i = 0; i < 16; ++i)
    {
        out[ i ] /= det;
    }
}

void Matrix::MakeIdentity()
{
    for (int i = 0; i < 15; ++i)
    {
        m[ i ] = 0;
    }

    m[ 0 ] = m[ 5 ] = m[ 10 ] = m[ 15 ] = 1;
}

void Matrix::MakeProjection( float fovDegrees, float aspect, float nearDepth, float farDepth )
{
    const float f = 1.0f / tanf( (0.5f * fovDegrees) * 3.14159265358979f / 180.0f );

    const float tmp = farDepth;
    farDepth = nearDepth;
    nearDepth = tmp;

    const float proj[] =
    {
        f / aspect, 0, 0, 0,
        0, -f, 0, 0,
        0, 0, farDepth / (nearDepth - farDepth), -1,
        0, 0, (nearDepth * farDepth) / (nearDepth - farDepth), 0
    };

    InitFrom( &proj[ 0 ] );
}

void Matrix::MakeProjection( float left, float right, float bottom, float top, float nearDepth, float farDepth )
{
    const float tmp = farDepth;
    farDepth = nearDepth;
    nearDepth = tmp;

    const float tx = -((right + left) / (right - left));
    const float ty = -((bottom + top) / (bottom - top));
    const float tz = nearDepth / (nearDepth - farDepth);

    const float proj[] =
    {
        2.0f / (right - left), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (bottom - top), 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f / (nearDepth - farDepth), 0.0f,
        tx, ty, tz, 1.0f
    };

    InitFrom( &proj[ 0 ] );
}

void Matrix::MakeRotationXYZ( float xDeg, float yDeg, float zDeg )
{
    const float deg2rad = 3.1415926535f / 180.0f;
    const float sx = sinf( xDeg * deg2rad );
    const float sy = sinf( yDeg * deg2rad );
    const float sz = sinf( zDeg * deg2rad );
    const float cx = cosf( xDeg * deg2rad );
    const float cy = cosf( yDeg * deg2rad );
    const float cz = cosf( zDeg * deg2rad );

    m[ 0 ] = cy * cz;
    m[ 1 ] = cz * sx * sy - cx * sz;
    m[ 2 ] = cx * cz * sy + sx * sz;
    m[ 3 ] = 0;
    m[ 4 ] = cy * sz;
    m[ 5 ] = cx * cz + sx * sy * sz;
    m[ 6 ] = -cz * sx + cx * sy * sz;
    m[ 7 ] = 0;
    m[ 8 ] = -sy;
    m[ 9 ] = cy * sx;
    m[ 10 ] = cx * cy;
    m[ 11 ] = 0;
    m[ 12 ] = 0;
    m[ 13 ] = 0;
    m[ 14 ] = 0;
    m[ 15 ] = 1;
}

void Matrix::Scale( float x, float y, float z )
{
    Matrix scale;
    scale.MakeIdentity();

    scale.m[ 0 ] = x;
    scale.m[ 5 ] = y;
    scale.m[ 10 ] = z;

    Multiply( *this, scale, *this );
}

void Matrix::Transpose( Matrix& out ) const
{
    teAssert( &out != this );

    out.m[ 0 ] = m[ 0 ];
    out.m[ 1 ] = m[ 4 ];
    out.m[ 2 ] = m[ 8 ];
    out.m[ 3 ] = m[ 12 ];
    out.m[ 4 ] = m[ 1 ];
    out.m[ 5 ] = m[ 5 ];
    out.m[ 6 ] = m[ 9 ];
    out.m[ 7 ] = m[ 13 ];
    out.m[ 8 ] = m[ 2 ];
    out.m[ 9 ] = m[ 6 ];
    out.m[ 10 ] = m[ 10 ];
    out.m[ 11 ] = m[ 14 ];
    out.m[ 12 ] = m[ 3 ];
    out.m[ 13 ] = m[ 7 ];
    out.m[ 14 ] = m[ 11 ];
    out.m[ 15 ] = m[ 15 ];
}

void Matrix::TransformDirection( const Vec3& dir, const Matrix& mat, Vec3* out )
{
    out->x = mat.m[ 0 ] * dir.x + mat.m[ 4 ] * dir.y + mat.m[ 8 ] * dir.z;
    out->y = mat.m[ 1 ] * dir.x + mat.m[ 5 ] * dir.y + mat.m[ 9 ] * dir.z;
    out->z = mat.m[ 2 ] * dir.x + mat.m[ 6 ] * dir.y + mat.m[ 10 ] * dir.z;
}

void Matrix::SetTranslation( const Vec3& translation )
{
    m[ 12 ] = translation.x;
    m[ 13 ] = translation.y;
    m[ 14 ] = translation.z;
}

void Matrix::Translate( const Vec3& v )
{
    Matrix translateMatrix;
    translateMatrix.MakeIdentity();

    translateMatrix.m[ 12 ] = v.x;
    translateMatrix.m[ 13 ] = v.y;
    translateMatrix.m[ 14 ] = v.z;

    Multiply( *this, translateMatrix, *this );
}

Quaternion::Quaternion( const struct Vec3& v, float aw )
{
    x = v.x;
    y = v.y;
    z = v.z;
    w = aw;
}

Vec3 Quaternion::operator*( const Vec3& vec ) const
{
    const Vec3 wComponent( w, w, w );
    const Vec3 two( 2.0f, 2.0f, 2.0f );

    const Vec3 vT = two * Vec3::Cross( Vec3( x, y, z ), vec );
    return vec + (wComponent * vT) + Vec3::Cross( Vec3( x, y, z ), vT );
}

Quaternion Quaternion::operator*( const Quaternion& aQ ) const
{
    return Quaternion( Vec3( w * aQ.x + x * aQ.w + y * aQ.z - z * aQ.y,
        w * aQ.y + y * aQ.w + z * aQ.x - x * aQ.z,
        w * aQ.z + z * aQ.w + x * aQ.y - y * aQ.x ),
        w * aQ.w - x * aQ.x - y * aQ.y - z * aQ.z );
}

void Quaternion::FromAxisAngle( const Vec3& axis, float angleDeg )
{
    const float angleRad = angleDeg * (3.1418693659f / 360.0f);
    const float sinAngle = sinf( angleRad );

    x = axis.x * sinAngle;
    y = axis.y * sinAngle;
    z = axis.z * sinAngle;
    w = cosf( angleRad );
}

void Quaternion::FindOrthonormals( const Vec3& normal, Vec3& orthonormal1, Vec3& orthonormal2 ) const
{
    Matrix orthoX( 90, 0, 0 );

    Vec3 ww;
    Matrix::TransformDirection( normal, orthoX, &ww );
    const float dot = Vec3::Dot( normal, ww );

    if (fabsf( dot ) > 0.6f)
    {
        Matrix orthoY( 0, 90, 0 );
        Matrix::TransformDirection( normal, orthoY, &ww );
    }

    ww = ww.Normalized();

    orthonormal1 = Vec3::Cross( normal, ww ).Normalized();
    orthonormal2 = Vec3::Cross( normal, orthonormal1 ).Normalized();
}

float Quaternion::FindTwist( const Vec3& axis ) const
{
    // Get the plane the axis is a normal of.
    Vec3 orthonormal1, orthonormal2;
    FindOrthonormals( axis, orthonormal1, orthonormal2 );

    Vec3 transformed = *this * orthonormal1;

    //project transformed vector onto plane
    Vec3 flattened = transformed - axis * Vec3::Dot( transformed, axis );
    flattened = flattened.Normalized();

    // get angle between original vector and projected transform to get angle around normal
    float dot = Vec3::Dot( orthonormal1, flattened );

    if (dot < -1)
    {
        dot = -1;
    }

    if (dot > 1)
    {
        dot = 1;
    }

    return acosf( dot );
}

void Quaternion::Normalize()
{
    const float mag2 = w * w + x * x + y * y + z * z;
    const float acceptableDelta = 0.00001f;

    if (fabsf( mag2 ) > acceptableDelta && fabsf( mag2 - 1.0f ) > acceptableDelta)
    {
        const float oneOverMag = 1.0f / sqrtf( mag2 );

        x *= oneOverMag;
        y *= oneOverMag;
        z *= oneOverMag;
        w *= oneOverMag;
    }
}

void Quaternion::FromMatrix( const Matrix& mat )
{
    float t;

    if (mat.m[ 10 ] < 0)
    {
        if (mat.m[ 0 ] > mat.m[ 5 ])
        {
            t = 1 + mat.m[ 0 ] - mat.m[ 5 ] - mat.m[ 10 ];
            *this = Quaternion( Vec3( t, mat.m[ 1 ] + mat.m[ 4 ], mat.m[ 8 ] + mat.m[ 2 ] ), mat.m[ 6 ] - mat.m[ 9 ] );
        }
        else
        {
            t = 1 - mat.m[ 0 ] + mat.m[ 5 ] - mat.m[ 10 ];
            *this = Quaternion( Vec3( mat.m[ 1 ] + mat.m[ 4 ], t, mat.m[ 6 ] + mat.m[ 9 ] ), mat.m[ 8 ] - mat.m[ 2 ] );
        }
    }
    else
    {
        if (mat.m[ 0 ] < -mat.m[ 5 ])
        {
            t = 1 - mat.m[ 0 ] - mat.m[ 5 ] + mat.m[ 10 ];
            *this = Quaternion( Vec3( mat.m[ 8 ] + mat.m[ 2 ], mat.m[ 6 ] + mat.m[ 9 ], t ), mat.m[ 1 ] - mat.m[ 4 ] );
        }
        else
        {
            t = 1 + mat.m[ 0 ] + mat.m[ 5 ] + mat.m[ 10 ];
            *this = Quaternion( Vec3( mat.m[ 6 ] - mat.m[ 9 ], mat.m[ 8 ] - mat.m[ 2 ], mat.m[ 1 ] - mat.m[ 4 ] ), t );
        }
    }

    const float factor = 0.5f / sqrtf( t );
    x *= factor;
    y *= factor;
    z *= factor;
    w *= factor;
}

void Quaternion::GetMatrix( Matrix& outMatrix ) const
{
    const float x2 = x * x;
    const float y2 = y * y;
    const float z2 = z * z;
    const float xy = x * y;
    const float xz = x * z;
    const float yz = y * z;
    const float wx = w * x;
    const float wy = w * y;
    const float wz = w * z;

    outMatrix.m[ 0 ] = 1 - 2 * (y2 + z2);
    outMatrix.m[ 1 ] = 2 * (xy - wz);
    outMatrix.m[ 2 ] = 2 * (xz + wy);
    outMatrix.m[ 3 ] = 0;
    outMatrix.m[ 4 ] = 2 * (xy + wz);
    outMatrix.m[ 5 ] = 1 - 2 * (x2 + z2);
    outMatrix.m[ 6 ] = 2 * (yz - wx);
    outMatrix.m[ 7 ] = 0;
    outMatrix.m[ 8 ] = 2 * (xz - wy);
    outMatrix.m[ 9 ] = 2 * (yz + wx);
    outMatrix.m[ 10 ] = 1 - 2 * (x2 + y2);
    outMatrix.m[ 11 ] = 0;
    outMatrix.m[ 12 ] = 0;
    outMatrix.m[ 13 ] = 0;
    outMatrix.m[ 14 ] = 0;
    outMatrix.m[ 15 ] = 1;
}

void teGetCorners( const Vec3& min, const Vec3& max, Vec3 outCorners[ 8 ] )
{
    outCorners[ 0 ] = Vec3( min.x, min.y, min.z );
    outCorners[ 1 ] = Vec3( max.x, min.y, min.z );
    outCorners[ 2 ] = Vec3( min.x, max.y, min.z );
    outCorners[ 3 ] = Vec3( min.x, min.y, max.z );
    outCorners[ 4 ] = Vec3( max.x, max.y, min.z );
    outCorners[ 5 ] = Vec3( min.x, max.y, max.z );
    outCorners[ 6 ] = Vec3( max.x, max.y, max.z );
    outCorners[ 7 ] = Vec3( max.x, min.y, max.z );
}

void GetMinMax( const Vec3* aPoints, unsigned count, Vec3& outMin, Vec3& outMax )
{
    outMin = aPoints[ 0 ];
    outMax = aPoints[ 0 ];

    for (int i = 1, s = count; i < s; ++i)
    {
        const Vec3& point = aPoints[ i ];

        if (point.x < outMin.x)
        {
            outMin.x = point.x;
        }

        if (point.y < outMin.y)
        {
            outMin.y = point.y;
        }

        if (point.z < outMin.z)
        {
            outMin.z = point.z;
        }

        if (point.x > outMax.x)
        {
            outMax.x = point.x;
        }

        if (point.y > outMax.y)
        {
            outMax.y = point.y;
        }

        if (point.z > outMax.z)
        {
            outMax.z = point.z;
        }
    }
}
