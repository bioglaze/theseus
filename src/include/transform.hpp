#pragma once

const struct Vec3& teTransformGetLocalPosition( unsigned index );
void teTransformSetLocalPosition( unsigned index, const Vec3& pos );
void teTransformSetLocalScale( unsigned index, float scale );
const struct Quaternion& teTransformGetLocalRotation( int index );
void teTransformSetLocalRotation( unsigned index, const Quaternion& rotation );
void teTransformLookAt( unsigned index, const Vec3& localPosition, const Vec3& center, const Vec3& up );
const struct Matrix& teTransformGetMatrix( unsigned index );
void teTransformMoveForward( unsigned index, float amount );
void teTransformMoveRight( unsigned index, float amount );
void teTransformMoveUp( unsigned index, float amount );
void teTransformOffsetRotate( unsigned index, const Vec3& axis, float angleDeg );
Vec3 teTransformGetViewDirection( unsigned index );
