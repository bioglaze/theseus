#pragma once

struct Quaternion
{
    Quaternion() noexcept : x( 0 ), y( 0 ), z( 0 ), w( 1 ) {}
    Quaternion( const struct Vec3& v, float aw );
    Vec3 operator*( const Vec3& vec ) const;
    Quaternion operator*( const Quaternion& aQ ) const;
    
    void FromAxisAngle( const Vec3& axis, float angleDeg );
    void FromMatrix( const struct Matrix& mat );
    float FindTwist( const Vec3& axis ) const;
    void FindOrthonormals( const Vec3& normal, Vec3& orthonormal1, Vec3& orthonormal2 ) const;
    void GetMatrix( Matrix& outMatrix ) const;
    void Normalize();

    float x, y, z, w;
};

