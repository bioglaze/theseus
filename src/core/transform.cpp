#include "transform.h"
#include "matrix.h"
#include "quaternion.h"
#include "vec3.h"

struct TransformImpl
{
    Matrix localMatrix;
    Matrix localToClipLeftEye;
    Matrix localToClipRightEye;
    Matrix localToViewLeftEye;
    Matrix localToViewRightEye;
    Matrix localToShadowClip;
    Quaternion localRotation;
    Vec3 localPosition;
    float localScale = 1;
};

TransformImpl transforms[ 10000 ];

const Vec3& teTransformGetLocalPosition( unsigned index )
{
    return transforms[ index ].localPosition;
}

void teTransformSetLocalScale( unsigned index, float scale )
{
    transforms[ index ].localScale = scale;
}

const Matrix& teTransformGetMatrix( unsigned index )
{
    return transforms[ index ].localMatrix;
}

void teTransformSetLocalPosition( unsigned index, const Vec3& pos )
{
    transforms[ index ].localPosition = pos;
}

const Quaternion& teTransformGetLocalRotation( int index )
{
    return transforms[ index ].localRotation;
}

void teTransformSetLocalRotation( unsigned index, const Quaternion& rotation )
{
    transforms[ index ].localRotation = rotation;
}

const Matrix& teTransformGetComputedLocalToShadowClipMatrix( unsigned index )
{
    return transforms[ index ].localToShadowClip;
}

void teTransformSetComputedLocalToShadowClipMatrix( unsigned index, const Matrix& localToShadowClip )
{
    transforms[ index ].localToShadowClip = localToShadowClip;
}

void teTransformGetComputedLocalToClipMatrix( unsigned index, Matrix& outLocalToClipLeftEye, Matrix& outLocalToClipRightEye )
{
    outLocalToClipLeftEye = transforms[ index ].localToClipLeftEye;
    outLocalToClipRightEye = transforms[ index ].localToClipRightEye;
}

void teTransformGetComputedLocalToViewMatrix( unsigned index, Matrix& outLocalToViewLeftEye, Matrix& outLocalToViewRightEye )
{
    outLocalToViewLeftEye = transforms[ index ].localToViewLeftEye;
    outLocalToViewRightEye = transforms[ index ].localToViewRightEye;
}

void TransformSolveLocalMatrix( unsigned index, bool isCamera )
{
    TransformImpl& ti = transforms[ index ];
    
    ti.localRotation.GetMatrix( ti.localMatrix );

    if (ti.localScale != 1)
    {
        ti.localMatrix.Scale( ti.localScale, ti.localScale, ti.localScale );
    }    

    Matrix translation;
    translation.SetTranslation( ti.localPosition );

    // FIXME: This is a hack to prevent camera's rotation to be weird
    if (isCamera)
    {
        Matrix::Multiply( translation, ti.localMatrix, ti.localMatrix );
    }
    else
    {
        Matrix::Multiply( ti.localMatrix, translation, ti.localMatrix );
    }
}

void teTransformSetComputedLocalToClip( unsigned index, const Matrix& localToClipLeftEye, const Matrix& localToClipRightEye )
{
    transforms[ index ].localToClipLeftEye = localToClipLeftEye;
    transforms[ index ].localToClipRightEye = localToClipRightEye;
}

void teTransformSetComputedLocalToView( unsigned index, const Matrix& localToViewLeftEye, const Matrix& localToViewRightEye )
{
    transforms[ index ].localToViewLeftEye = localToViewLeftEye;
    transforms[ index ].localToViewRightEye = localToViewRightEye;
}

Vec3 teTransformGetViewDirection( unsigned index )
{
    TransformImpl& ti = transforms[ index ];

    Matrix view;
    ti.localRotation.GetMatrix( view );

    Matrix translation;
    translation.SetTranslation( -ti.localPosition );
    Matrix::Multiply( translation, view, view );
    
    return Vec3( view.m[ 2 ], view.m[ 6 ], view.m[ 10 ] ).Normalized();
}

static float MyAbs( float f )
{
    return f < 0 ? -f : f;
}

static bool IsAlmost( float f1, float f2 )
{
    return MyAbs( f1 - f2 ) < 0.0001f;
}

void teTransformOffsetRotate( unsigned index, const Vec3& axis, float angleDeg )
{
    Quaternion rot;
    rot.FromAxisAngle( axis, angleDeg );

    Quaternion newRotation;

    if (IsAlmost( axis.y, 0 ))
    {
        newRotation = transforms[ index ].localRotation * rot;
    }
    else
    {
        newRotation = rot * transforms[ index ].localRotation;
    }

    newRotation.Normalize();

    if ((IsAlmost( axis.x, 1 ) || IsAlmost( axis.x, -1 )) && IsAlmost( axis.y, 0 ) && IsAlmost( axis.z, 0 ) &&
        newRotation.FindTwist( Vec3( 1.0f, 0.0f, 0.0f ) ) > 0.9999f)
    {
        return;
    }

    transforms[ index ].localRotation = newRotation;
}

void teTransformLookAt( unsigned index, const Vec3& localPosition, const Vec3& center, const Vec3& up )
{
    Matrix lookAt;
    lookAt.MakeLookAtLH( localPosition, center, up );
    transforms[ index ].localRotation.FromMatrix( lookAt );
    transforms[ index ].localPosition = localPosition;
}

void teTransformMoveForward( unsigned index, float amount )
{
    if (!IsAlmost( amount, 0 ))
    {
        transforms[ index ].localPosition += transforms[ index ].localRotation * Vec3( 0, 0, amount );
    }
}

void teTransformMoveRight( unsigned index, float amount )
{
    if (!IsAlmost( amount, 0 ))
    {
        transforms[ index ].localPosition += transforms[ index ].localRotation * Vec3( amount, 0, 0 );
    }
}

void teTransformMoveUp( unsigned index, float amount )
{
    transforms[ index ].localPosition.y += amount;
}
