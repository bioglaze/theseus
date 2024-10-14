#pragma once

struct Vec3
{
    float x, y, z;

    Vec3() noexcept : x( 0 ), y( 0 ), z( 0 ) {}
    Vec3( float ax, float ay, float az) : x( ax ), y( ay ), z( az ) {}
    Vec3 operator*( const Vec3& v ) const { return { x * v.x, y * v.y, z * v.z }; }
    Vec3 operator+( const Vec3& v ) const { return { x + v.x, y + v.y, z + v.z }; }
    Vec3 operator*( float f ) const { return Vec3( x * f, y * f, z * f ); }

    Vec3& operator+=( const Vec3& v )
    {
        x += v.x;
        y += v.y;
        z += v.z;
        
        return *this;
    }

    Vec3 operator-() const
    {
        return Vec3( -x, -y, -z );
    }

    Vec3 operator-( const Vec3& v ) const
    {
        return Vec3( x - v.x, y - v.y, z - v.z );
    }

    Vec3 operator/( float f ) const noexcept
    {
        const float inv = 1.0f / f;
        return Vec3( x * inv, y * inv, z * inv );
    }

    Vec3 Normalized() const;
    void Normalize();

    static Vec3 Cross( const Vec3& v1, const Vec3& v2 )
    {
        return Vec3( v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x );
    }

    static float Dot( const Vec3& a, const Vec3& b )
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
};

struct Vec4
{
    Vec4() noexcept : x( 0 ), y( 0 ), z( 0 ), w( 0 ) {}
    Vec4( float ax, float ay, float az, float aw ) : x( ax ), y( ay ), z( az ), w( aw ) {}

    Vec4& operator+=( const Vec4& v )
    {
        x += v.x;
        y += v.y;
        z += v.z;

        return *this;
    }

    Vec4& operator-=( const Vec4& v )
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;

        return *this;
    }

    Vec4 operator*( float f ) const
    {
        return Vec4( x * f, y * f, z * f, w * f );
    }

    static float Dot( const Vec4& a, const Vec4& b )
    {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }

    void Normalize();

    float x, y, z, w;
};
