#pragma once

struct alignas( 16 ) Matrix
{
    static void Invert( const Matrix& matrix, Matrix& out );
    static void InverseTranspose( const float m[ 16 ], float* out );
    static void Multiply( const Matrix& ma, const Matrix& mb, Matrix& out );
    static void TransformDirection( const struct Vec3& dir, const Matrix& mat, Vec3* out );
    static void TransformPoint( const Vec3& point, const Matrix& mat, Vec3& out );

    Matrix() noexcept { MakeIdentity(); }
    Matrix( float xDeg, float yDeg, float zDeg ) { MakeRotationXYZ( xDeg, yDeg, zDeg ); }

    void InitFrom( const float* mat );
    void MakeIdentity();
    void MakeRotationXYZ( float xDeg, float yDeg, float zDeg );
    void MakeProjection( float fovDegrees, float aspect, float nearDepth, float farDepth );
    void MakeProjection( float left, float right, float bottom, float top, float nearDepth, float farDepth );
    void MakeLookAtLH( const struct Vec3& eye, const Vec3& center, const Vec3& up );
    void Scale( float x, float y, float z );
    void SetTranslation( const Vec3& translation );
    void Transpose( Matrix& out ) const;
    void Translate( const Vec3& );

    float m[ 16 ];
};
